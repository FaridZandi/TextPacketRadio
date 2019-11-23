#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Keypad.h>
#include <RH_NRF24.h>

int t9_text_input_index;
char t9_text_input[100];
char t9_current_key_index = 0;
char t9_last_key;
unsigned long t9_last_key_time;
int t9_max_input_interval = 1000;
long t9_number_input;

String notification_text;
int notification_issue_time;

RH_NRF24 nrf24(9, 10);
int nrf_channel;
RH_NRF24::DataRate nrf_data_rate;
RH_NRF24::TransmitPower nrf_transmit_power;

RH_NRF24::TransmitPower nrf_tx_power_options_value[] = {
        RH_NRF24::TransmitPower0dBm,
        RH_NRF24::TransmitPowerm6dBm,
        RH_NRF24::TransmitPowerm12dBm,
        RH_NRF24::TransmitPowerm18dBm,
};

RH_NRF24::DataRate nrf_data_rate_options_value[] = {
        RH_NRF24::DataRate1Mbps,       ///< 1 Mbps
        RH_NRF24::DataRate2Mbps,       ///< 2 Mbps
        RH_NRF24::DataRate250kbps      ///< 250 kbps
};

int nrf_channel_options_value[] = {
        1,
        2
};


enum menu_state {
    MainMenu,
    Settings,
    SettingChannelSelect,
    SettingDataRateSelect,
    SettingTXPower,
    SendText,
    ReceiveText,
    GetDestAddr,
    SendingMessage,
};

int current_menu_state = menu_state::MainMenu;


const byte ROWS = 4;
const byte COLS = 4;
char keys[4][4] = {
        {'b', 'A', 'D', '1'},
        {'G', 'J', 'M', '2'},
        {'P', 'T', 'W', '3'},
        {'*', ' ', '#', '4'}
};
byte rowPins[ROWS] = {A0, A1, A2, A3};
byte colPins[COLS] = {A4, A5, 2, 3};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Adafruit_PCD8544 display = Adafruit_PCD8544(8, 7, 6, 5, 4);

char get_cycle_for_key();

void reset_t9_text_input();

void reset_t9_number_input();

void initialize_display();

void display_text(const char *text);

void refresh_display();

void t9_text_input_handle(char key);

void t9_number_input_handle(char key);

int get_keypad_number(char key);

void reset_menu_message();

void send_notif(String notif);

void setup() {
    nrf_transmit_power = RH_NRF24::TransmitPower0dBm;
    nrf_data_rate = RH_NRF24::DataRate2Mbps;
    nrf_channel = 1;
    if (!nrf24.init())
        Serial.println("Init failed!");
    if (!nrf24.setChannel(nrf_channel))
        Serial.println("setChannel failed");
    if (!nrf24.setRF(nrf_data_rate, nrf_transmit_power))
        Serial.println("setRF failed");
    Serial.begin(9600);

    keypad.addEventListener(keypadEvent); //new event

    reset_menu_message();
    reset_t9_text_input();
    reset_t9_number_input();

    initialize_display();
}

void loop() {
    keypad.getKey();
    delay(10);
    refresh_display();
    Serial.println(current_menu_state);
}

void reset_menu_message() {
    notification_issue_time = -100000;
}


void keypadEvent(KeypadEvent key) {
    int num = get_keypad_number(key);

    switch (keypad.getState()) {
        case PRESSED:
            if (current_menu_state == MainMenu) {
                switch (num) {
                    case 1:
                        current_menu_state = Settings;
                        break;
                    case 2:
                        reset_t9_text_input();
                        current_menu_state = SendText;
                        break;
                    case 3:
                        current_menu_state = ReceiveText;
                }
            } else if (current_menu_state == Settings) {
                switch (num) {
                    case 1:
                        current_menu_state = SettingDataRateSelect;
                        break;
                    case 2:
                        current_menu_state = SettingTXPower;
                        break;
                    case 3:
                        current_menu_state = SettingChannelSelect;
                        break;
                    case 0:
                        current_menu_state = MainMenu;
                }
            } else if (current_menu_state == SettingChannelSelect) {
                switch (num) {
                    case 1:
                    case 2:
                        nrf_channel = nrf_channel_options_value[num - 1];
                        
                        if (!nrf24.setChannel(nrf_channel)){
                            send_notif(String("Failed to set the channel."));
                        } else {
                            send_notif(String("Channel set to option ") + String(num) + String(" successfully."));
                        }
                    case 0:
                        current_menu_state = Settings;
                }
            } else if (current_menu_state == SettingDataRateSelect) {
                switch (num) {
                    case 1:
                    case 2:
                    case 3:
                        nrf_data_rate = nrf_data_rate_options_value[num - 1];
                        if (!nrf24.setRF(nrf_data_rate, nrf_transmit_power))
                            Serial.println("setRF failed");
                    case 0:
                        current_menu_state = Settings;
                }
            } else if (current_menu_state == SettingTXPower) {
                switch (num) {
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                        nrf_transmit_power = nrf_tx_power_options_value[num - 1];
                        if (!nrf24.setRF(nrf_data_rate, nrf_transmit_power))
                            Serial.println("setRF failed");
                    case 0:
                        current_menu_state = Settings;
                }
            } else if (current_menu_state == SendText) {
                if (key == '1') {
                    current_menu_state = GetDestAddr;
                } else {
                    t9_text_input_handle(key);
                }
            } else if (current_menu_state == GetDestAddr) {
                if (key == '1') {
                    current_menu_state = SendingMessage;
                    int len = 0;
                    for (int i = 0; i < 100; i++) {
                        if (t9_text_input[i] != '\0')
                            len++;
                        else break;
                    }
                    nrf24.send((unsigned char *) t9_text_input, len * sizeof(char));
                    nrf24.waitPacketSent();
                    delay(1000);
                    current_menu_state = MainMenu;
                } else {
                    t9_number_input_handle(key);
                }
            }
            break;
        case RELEASED:
            t9_last_key_time = millis();
            t9_last_key = key;
            if (get_cycle_for_key(t9_last_key) == 1) {
                t9_last_key = -1;
            }
            break;
        case HOLD:
            break;
        case IDLE:
            break;
    }
}

void send_notif(String notif) {
    notification_text = notif;
    notification_issue_time = millis();
}

char get_cycle_for_key(char key) {
    char cycle;
    if (key == 'P' ||
        key == 'W') {
        cycle = 4;
    } else if (key == '1' ||
               key == '2' ||
               key == '3' ||
               key == '4' ||
               key == '*' ||
               key == '#' ||
               key == ' ') {
        cycle = 1;
    } else {
        cycle = 3;
    }
    return cycle;
}


void display_text(const char *text) {
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(0, 0);
    display.println(text);
    display.display();
    display.clearDisplay();
}

void reset_t9_text_input() {
    t9_last_key = -1;
    t9_text_input_index = -1;
    int i;
    for (i = 0; i < 100; i++) {
        t9_text_input[i] = '\0';
    }
}

void initialize_display() {
    display.begin();
    display.setContrast(67);
    display.clearDisplay();
}

void refresh_display() {
    String text;

    if(millis() - notification_issue_time < t9_max_input_interval){
        text = notification_text;
    } else {
        switch (current_menu_state) {
            case MainMenu:
                text = "Main Menu!\n\n1.Settings\n2.Send Text\n3.Receive Text";
                break;
            case Settings:
                text = "Settings: \n\n1.Data Rate\n2.TX Power\n3.Channel\n0. Back";
                break;
            case SettingTXPower:
                text = "TX Power: \n1. 0 dBm\n2. -6 dBm\n3. -12 dBm\n4. -18 dBm\n0. Back";
                break;
            case SettingDataRateSelect:
                text = "Data Rate: \n\n1. 1 Mbps\n2. 2 Mbps\n3. 250 kbps\n0. Back";
                break;
            case SettingChannelSelect:
                text = "Channel: \n\n1. 1 \n2. 2\n0. Back";
                break;
            case SendText:
                text = String("Enter Message:\n\n") + String(t9_text_input);
                break;
            case ReceiveText:
                break;
            case GetDestAddr:
                text = String("Enter Dest Address:\n\n") + String(t9_number_input);
                break;
            case SendingMessage:
                text = "Sending Message! \n\nPlease Wait";
                break;
            default:
                text = "invalid state! please implement this";
        }
    }
    display_text(text.c_str());
}


void t9_text_input_handle(char key) {
    unsigned long present = millis();
    unsigned long t9_text_input_interval = present - t9_last_key_time;

    if (key == '2') {
        if (t9_text_input_index >= 0) {
            t9_text_input[t9_text_input_index] = '\0';
        }
        t9_text_input_index--;
        if (t9_text_input_index < -1) {
            t9_text_input_index = -1;
        }
    } else {
        if (t9_last_key == key && t9_text_input_interval < t9_max_input_interval) {
            t9_current_key_index++;
            char cycle = get_cycle_for_key(key);
            char currentKey = t9_last_key + (t9_current_key_index % cycle);
            t9_text_input[t9_text_input_index] = currentKey;
        } else {
            t9_text_input_index++;
            t9_text_input[t9_text_input_index] = key;
            t9_current_key_index = 0;
        }
    }
}

void reset_t9_number_input() {
    t9_number_input = 0;
}

void t9_number_input_handle(char key) {
    if (key == '2') {
        t9_number_input /= 10;
    } else {
        int current_number = get_keypad_number(key);
        if (current_number >= 0) {
            t9_number_input *= 10;
            t9_number_input += current_number;
        }
    }
}

int get_keypad_number(char key) {
    switch (key) {
        case 'b':
            return 1;
        case 'A':
            return 2;
        case 'D':
            return 3;
        case 'G':
            return 4;
        case 'J':
            return 5;
        case 'M':
            return 6;
        case 'P':
            return 7;
        case 'T':
            return 8;
        case 'W':
            return 9;
        case ' ':
            return 0;
        default:
            return -1;
    }
}


//SYSTEM_THREAD(ENABLED);

#if defined(ARDUINO) 
SYSTEM_MODE(SEMI_AUTOMATIC); 
#endif

#define MIN_CONN_INTERVAL          0x0028 // 50ms. 
#define MAX_CONN_INTERVAL          0x0190 // 500ms. 
#define SLAVE_LATENCY              0x0000 // No slave latency. 
#define CONN_SUPERVISION_TIMEOUT   0x03E8 // 10s. 

#define BLE_PERIPHERAL_APPEARANCE  BLE_APPEARANCE_UNKNOWN

#define BLE_DEVICE_NAME            "LukeOscarAndre"

#define CHARACTERISTIC1_MAX_LEN    7
#define CHARACTERISTIC2_MAX_LEN    7

#define PWM_PIN                    D0
#define BUTTON_PIN                 D1
#define ANALOG_POT_PIN             A0
#define ANALOG_PR_PIN              A1

/******************************************************
 *               Variable Definitions
 ******************************************************/
static uint8_t service1_uuid[16]    = { 0x71,0x3d,0x00,0x00,0x50,0x3e,0x4c,0x75,0xba,0x94,0x31,0x48,0xf1,0x8d,0x94,0x1e };
static uint8_t service1_tx_uuid[16] = { 0x71,0x3d,0x00,0x03,0x50,0x3e,0x4c,0x75,0xba,0x94,0x31,0x48,0xf1,0x8d,0x94,0x1e };
static uint8_t service1_rx_uuid[16] = { 0x71,0x3d,0x00,0x02,0x50,0x3e,0x4c,0x75,0xba,0x94,0x31,0x48,0xf1,0x8d,0x94,0x1e };

static uint8_t  appearance[2] = { 
  LOW_BYTE(BLE_PERIPHERAL_APPEARANCE), 
  HIGH_BYTE(BLE_PERIPHERAL_APPEARANCE) 
};

static uint8_t  change[4] = {
  0x00, 0x00, 0xFF, 0xFF
};

static uint8_t  conn_param[8] = {
  LOW_BYTE(MIN_CONN_INTERVAL), HIGH_BYTE(MIN_CONN_INTERVAL), 
  LOW_BYTE(MAX_CONN_INTERVAL), HIGH_BYTE(MAX_CONN_INTERVAL), 
  LOW_BYTE(SLAVE_LATENCY), HIGH_BYTE(SLAVE_LATENCY), 
  LOW_BYTE(CONN_SUPERVISION_TIMEOUT), HIGH_BYTE(CONN_SUPERVISION_TIMEOUT)
};

static advParams_t adv_params = {
  .adv_int_min   = 0x0030,
  .adv_int_max   = 0x0030,
  .adv_type      = BLE_GAP_ADV_TYPE_ADV_IND,
  .dir_addr_type = BLE_GAP_ADDR_TYPE_PUBLIC,
  .dir_addr      = {0,0,0,0,0,0},
  .channel_map   = BLE_GAP_ADV_CHANNEL_MAP_ALL,
  .filter_policy = BLE_GAP_ADV_FP_ANY
};

static uint8_t adv_data[] = {
  0x02,
  BLE_GAP_AD_TYPE_FLAGS,
  BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE, 
  
  0x08,
  BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME,
  'B','i','s','c','u','i','t', 
  
  0x11,
  BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE,
  0x1e,0x94,0x8d,0xf1,0x48,0x31,0x94,0xba,0x75,0x4c,0x3e,0x50,0x00,0x00,0x3d,0x71 
};

static uint16_t character1_handle = 0x0000;
static uint16_t character2_handle = 0x0000;

static uint8_t characteristic1_data[CHARACTERISTIC1_MAX_LEN] = { 0x01 };
static uint8_t characteristic2_data[CHARACTERISTIC2_MAX_LEN] = { 0x00 };

static btstack_timer_source_t characteristic2;

//My Vars
static uint8_t led_value = 0;
static int timer_rate = 0; //if set to zero, do not reschedule timer

/******************************************************
 *               Function Definitions
 ******************************************************/
/**
 * @brief Connect handle.
 *
 * @param[in]  status   BLE_STATUS_CONNECTION_ERROR or BLE_STATUS_OK.
 * @param[in]  handle   Connect handle.
 *
 * @retval None
 */
void deviceConnectedCallback(BLEStatus_t status, uint16_t handle) {
  switch (status) {
    case BLE_STATUS_OK:
      Serial.println("Device connected!");
      break;
    default: break;
  }
}

/**
 * @brief Disconnect handle.
 *
 * @param[in]  handle   Connect handle.
 *
 * @retval None
 */
void deviceDisconnectedCallback(uint16_t handle) {
  Serial.println("Disconnected.");
}

/**
 * @brief Callback for writting event.
 *
 * @param[in]  value_handle  
 * @param[in]  *buffer       The buffer pointer of writting data.
 * @param[in]  size          The length of writting data.   
 *
 * @retval 
 */
int gattWriteCallback(uint16_t value_handle, uint8_t *buffer, uint16_t size) {
  Serial.print("Write value handler: ");
  Serial.println(value_handle, HEX);

  if (character1_handle == value_handle) {
    memcpy(characteristic1_data, buffer, CHARACTERISTIC1_MAX_LEN);
    Serial.print("Characteristic1 write value: ");
    for (uint8_t index = 0; index < CHARACTERISTIC1_MAX_LEN; index++) {
      Serial.print(characteristic1_data[index], HEX);
      Serial.print(" ");
    }
    Serial.println(" ");
    
    if (characteristic1_data[0] == 0x01) { // Command is to control digital out pin
      led_value = characteristic1_data[1];
      analogWrite(PWM_PIN, led_value);
      Serial.println("BLE Set Led Level" + String(led_value));
    }
    else if (characteristic1_data[0] == 0x02) { //poll the led value once
      handle_button();
    }
    else if (characteristic1_data[0] == 0x03) { // set photoresistor polling rate
      timer_rate = characteristic1_data[1] * 10; //*10ms
      if(timer_rate<50){
        ble.removeTimer(&characteristic2);
      } else {
        ble.setTimer(&characteristic2, timer_rate);
        ble.addTimer(&characteristic2);
      }
    }
  }
  return 0;
}

void characteristic2_notify_led_status() {
  characteristic2_data[0] = (0x02); //"header" for sending a one byte int with the LED's value
  characteristic2_data[1] = (led_value);
  characteristic2_data[2] = (0x00);
  if (ble.attServerCanSendPacket()){
    ble.sendNotify(character2_handle, characteristic2_data, CHARACTERISTIC2_MAX_LEN);
    Serial.println("send led status");
  }
}


/**
 * @brief Timer task for sending status change to client.
 *
 * @param[in]  *ts   
 *
 * @retval None
 */
static unsigned long lastread_time = 0;
static unsigned long time_difference = 0;
static void  characteristic2_notify(btstack_timer_source_t *ts) {
    RGB.color(200,200,200);
    uint16_t value = analogRead(ANALOG_PR_PIN);
    if(lastread_time == 0){
      time_difference = 0;
    } else {
      time_difference = millis()-lastread_time;
    }
    lastread_time = millis();
    
    characteristic2_data[0] = (0x01);
    characteristic2_data[1] = (value >> 8);
    characteristic2_data[2] = (value);
    characteristic2_data[3] = (time_difference >> 24);
    characteristic2_data[4] = (time_difference >> 16);
    characteristic2_data[5] = (time_difference >> 8);
    characteristic2_data[6] = (time_difference);

    if (ble.attServerCanSendPacket()){
      ble.sendNotify(character2_handle, characteristic2_data, CHARACTERISTIC2_MAX_LEN);
    }
  // Restart timer.
  // TODO: what is an appropriate time?
  ble.setTimer(ts, timer_rate);
  ble.addTimer(ts);
  delay(25);
  RGB.color(0,0,0);
}

/**
 * @brief Setup.
 */
void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("Simple Controls demo.");

    // Initialize all peripherals
  RGB.control(true);
  pinMode(PWM_PIN, OUTPUT);
  pinMode(ANALOG_POT_PIN, INPUT);
  pinMode(ANALOG_PR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  attachInterrupt(BUTTON_PIN, handle_button, FALLING);
  
  //ble.debugLogger(true);
  // Initialize ble_stack.
  ble.init();

  // Register BLE callback functions
  ble.onConnectedCallback(deviceConnectedCallback);
  ble.onDisconnectedCallback(deviceDisconnectedCallback);
  ble.onDataWriteCallback(gattWriteCallback);

  // Add GAP service and characteristics
  ble.addService(BLE_UUID_GAP);
  ble.addCharacteristic(BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME, ATT_PROPERTY_READ|ATT_PROPERTY_WRITE, (uint8_t*)BLE_DEVICE_NAME, sizeof(BLE_DEVICE_NAME));
  ble.addCharacteristic(BLE_UUID_GAP_CHARACTERISTIC_APPEARANCE, ATT_PROPERTY_READ, appearance, sizeof(appearance));
  ble.addCharacteristic(BLE_UUID_GAP_CHARACTERISTIC_PPCP, ATT_PROPERTY_READ, conn_param, sizeof(conn_param));

  // Add GATT service and characteristics
  ble.addService(BLE_UUID_GATT);
  ble.addCharacteristic(BLE_UUID_GATT_CHARACTERISTIC_SERVICE_CHANGED, ATT_PROPERTY_INDICATE, change, sizeof(change));

  // Add user defined service and characteristics
  ble.addService(service1_uuid);
  character1_handle = ble.addCharacteristicDynamic(service1_tx_uuid, ATT_PROPERTY_NOTIFY|ATT_PROPERTY_WRITE|ATT_PROPERTY_WRITE_WITHOUT_RESPONSE, characteristic1_data, CHARACTERISTIC1_MAX_LEN);
  character2_handle = ble.addCharacteristicDynamic(service1_rx_uuid, ATT_PROPERTY_NOTIFY, characteristic2_data, CHARACTERISTIC2_MAX_LEN);

  // Set BLE advertising parameters
  ble.setAdvertisementParams(&adv_params);

  // Set BLE advertising data
  ble.setAdvertisementData(sizeof(adv_data), adv_data);

  // BLE peripheral starts advertising now.
  ble.startAdvertising();
  Serial.println("BLE start advertising.");

  // Start a task to check status.
  characteristic2.process = &characteristic2_notify;
}

void handle_button()
{
 static unsigned long last_interrupt_time = 0; //save state locally
 unsigned long interrupt_time = millis();
 // If interrupts come faster than 250ms, assume it's a bounce and ignore
 if (interrupt_time - last_interrupt_time > 250){
  led_value = map(analogRead(ANALOG_POT_PIN),0,4095,0,255);
  if(led_value<5){
    led_value = 0;
  }
  analogWrite(PWM_PIN, led_value);
  characteristic2_notify_led_status(); //send the LED value to the iPhone
 }
 last_interrupt_time = interrupt_time;
}

/**
 * @brief Loop.
 */
void loop() {
    
}

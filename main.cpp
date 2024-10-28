#include "mbed.h"
#include "I2C.h"
#include "ThisThread.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h" 
#include <cstring>
#include <cstdio>
#include <stdio.h>

//Definiciones
#define tiempo_muestreo   2s
#define escritura     0x40
#define poner_brillo  0x88
#define dir_display   0xC0
#define AHT15_ADDRESS 0x38 << 1

// Prototipos
void send_byte(char data);
void send_data(int number);
void condicion_start(void);
void condicion_stop(void);

//Pines y puertos 
BufferedSerial serial(USBTX, USBRX);
I2C i2c (D14,D15);
Adafruit_SSD1306_I2c oled (i2c, D0);
DigitalOut sclk(D2);  // Clock pin
DigitalInOut dio(D3); // Data pin
DigitalOut led(LED1);//Led para saber que esta funcionando el codigo
AnalogIn ntc_pin(A0);
AnalogIn lm35_pin(A1);


// Variables globales
const char digitToSegment[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
const float VADC = 3.3; 
const float RRef = 10000.0;  // Resistencia de referencia
const float A = 1.009249522e-03; // Coeficientes Steinhart-Hart
const float B = 2.378405444e-04;
const float C = 2.019202697e-07;
float Vin=0.0;
int ent=0;
int dec=0;
char men[40];
char data[6];
char comando[3]= {0xE1, 0x08,0x00};

void aht15_init() {
    char cmd[3] = {0xE1, 0x08, 0x00};  // Comando de inicialización
    i2c.write(AHT15_ADDRESS, cmd, 3);
    ThisThread::sleep_for(20ms);  // Esperar a que el sensor esté listo
}

bool read_aht15(float &temperature) {
    char cmd = 0xAC;  // Comando para iniciar la medición
    i2c.write(AHT15_ADDRESS, &cmd, 1);
    ThisThread::sleep_for(80ms);  // Espera de medición
    
    if (i2c.read(AHT15_ADDRESS, data, 6) == 0) {  // Leer 6 bytes de datos
        uint32_t raw_temp = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
        temperature = ((float)raw_temp / 1048576.0) * 200.0 - 50.0;  // Conversión de temperatura
        return true;
    }
    return false;
}

// Función para ordenar un array usando el algoritmo de burbuja
void bubble_sort(float arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                // Intercambiar los elementos si están en el orden incorrecto
                float temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

const char *mensaje_inicio = "Arranque del programa\n\r";

int main() {

    oled.begin();
    oled.setTextSize(1);
    oled.setTextColor(1);
    oled.display();
    ThisThread::sleep_for(3000ms);
    // Limpiar Pantalla
    oled.clearDisplay();
    oled.display();
    oled.printf( "Test\r\n" );
    oled.display();

    aht15_init();

    serial.write(mensaje_inicio, strlen(mensaje_inicio));

    while (true) {
    const int num_samples = 10; // Número de muestras a tomar
    float ntc_temperatures[num_samples]; // Array para temperaturas NTC
    float lm35_temperatures[num_samples]; // Array para temperaturas LM35
    float total_ntc = 0.0f;  // Suma de temperaturas NTC
    float total_lm35 = 0.0f;
    float Aht_temperatures[num_samples]; // Array para temperaturas LM35
    float total_Aht = 0.0f; // Suma de temperaturas LM35

    // Capturar 10 muestras del NTC
    for (int i = 0; i < num_samples; i++) {
        // Calcular la resistencia del NTC
        float RNtc = RRef * (VADC / ntc_pin.read() - 1.0);
        float logR = log(RNtc);
        float T_inv = A + B * logR + C * logR * logR * logR; // 1/T en Kelvin
        float TempK = 1.0 / T_inv; // Temperatura en Kelvin

        // Convertir a Celsius
        float TempC = TempK - 273.15;

        // Almacenar la temperatura en el array y acumular
        ntc_temperatures[i] = TempC*(-1);
        total_ntc += TempC*(-1);

        // Pequeña espera entre lecturas (por ejemplo, 10 ms)
        thread_sleep_for(100);
    }

    // Capturar 10 muestras del LM35
    for (int i = 0; i < num_samples; i++) {
        // Leer el voltaje del sensor LM35
        float voltage = lm35_pin.read() * 3.3f; // Asumiendo un VCC de 3.3V
        float temperatureLm35 = voltage * 100.0f;   // El LM35 entrega 10mV por °C
        
        // Almacenar la temperatura en el array y acumular
        lm35_temperatures[i] = temperatureLm35;
        total_lm35 += temperatureLm35;
        
        // Pequeña espera entre lecturas (por ejemplo, 10 ms)
        thread_sleep_for(100);
    }

    for (int i = 0; i < num_samples; i++) {
        float temperatura = 0.0;
        read_aht15(temperatura);
        // Leer el voltaje del sensor LM35        
        // Almacenar la temperatura en el array y acumular
        Aht_temperatures[i] = temperatura;
        total_Aht += temperatura;
        
        // Pequeña espera entre lecturas (por ejemplo, 10 ms)
        thread_sleep_for(100);
    }

    // Ordenar las temperaturas usando el algoritmo de burbuja
    bubble_sort(ntc_temperatures, num_samples);
    bubble_sort(lm35_temperatures, num_samples);
    bubble_sort(Aht_temperatures, num_samples);

    // Calcular los promedios
    float average_ntc = total_ntc / num_samples;
    int entero_1 = floor(average_ntc);
    int decimal_1 = abs((average_ntc-entero_1)*100);
    float average_lm35 = total_lm35 / num_samples;
    int entero_2 = floor(average_lm35);
    int decimal_2 = abs((average_lm35-entero_2)*100);
    float average_aht = total_Aht / num_samples;
    int entero_4 = floor(average_aht);
    int decimal_4 = abs((average_aht-entero_4)*100);
    float combined_average = (average_ntc + average_aht) / 2.0;
    int entero_3 = floor(combined_average);
    int decimal_3 = abs((combined_average-entero_3)*100);
    float errorabs = fabs(average_ntc-average_aht);
    int entero_5 = floor(errorabs);
    int decimal_5 = abs((errorabs-entero_5)*100);
    float ErrorRelaNtc = (errorabs/average_ntc)*100;
    int entero_6 = floor(ErrorRelaNtc);
    int decimal_6 = abs((ErrorRelaNtc-entero_6)*100);
    float ErrorRelaAHT = (errorabs/average_aht)*100;
    int entero_7 = floor(ErrorRelaAHT);
    int decimal_7 = abs((ErrorRelaAHT-entero_7)*100);
    // Mostrar los promedios
    printf("\nTemperatura promedio NTC: %d.%02d °C\n", entero_1,decimal_1);
    printf("Temperatura promedio LM35: %d.%02d °C\n", entero_2,decimal_2);
    printf("Temperatura promedio AHT: %d.%02d °C\n", entero_4,decimal_4);
    printf("Promedio combinado de NTC y LM35: %d.%02d °C\n", entero_3,decimal_3);  
    printf("Error Absoluto: %d.%02d °C\n", entero_5,decimal_5);
    printf("Error Relativo NTC: %d.%02d °C\n", entero_6,decimal_6);
    printf("Error Relativo AHT: %d.%02d °C\n", entero_7,decimal_7);    
    send_data(combined_average*100);

    oled.clearDisplay();
        oled.display();
        // formateo de la cadena de caracteres
        sprintf(men, "TemperaturaNTC:\n\r %01u.%04u Celsius \n\r",entero_1, decimal_1);
        oled.setTextCursor(0,2);
        oled.printf("%s", men);
        oled.display();
        // impresion puerto serie
        serial.write(men, strlen(men));
        //Lectura sensor I2C
        // Leer el registro de temperatura
        comando[0] = 0; // Registro de temperatura
        int16_t temp = (data[0] << 4) | (data[1] >> 4);
        ThisThread::sleep_for(tiempo_muestreo);

        oled.clearDisplay();
        oled.display();
        sprintf(men, "Temperatura LM35:\n\r %01u.%04u Celsius\n\r", entero_2, decimal_2);
        oled.setTextCursor(0,2);
        oled.printf("%s", men);
        oled.display();
        // impresion puerto serie
        serial.write(men, strlen(men));    

        ThisThread::sleep_for(tiempo_muestreo);

        float temperatura = 0.0;
        if (read_aht15(temperatura)) {
            ent = int(temperatura);
            dec = int((temperatura - ent) * 10000);

            // Mostrar temperatura en la pantalla OLED
            oled.clearDisplay();
            oled.display();
            sprintf(men, "TemperaturaAHT:\n\r %01u.%04u Celsius\n\r", ent, dec);
            oled.setTextCursor(0, 2);
            oled.printf(men);
            oled.display();
            // Impresión puerto serie
            serial.write(men, strlen(men));
        } else {
            // En caso de error
            oled.clearDisplay();
            oled.display();
            sprintf(men, "Error al leer AHT15\n\r");
            oled.setTextCursor(0, 2);
            oled.printf(men);
            oled.display();
            // Impresión puerto serie
            serial.write(men, strlen(men));
        }

        ThisThread::sleep_for(tiempo_muestreo);

        oled.clearDisplay();
        oled.display();
        sprintf(men,"ErrorAbsoluto: %d.%02d Celsius\n", entero_5,decimal_5);
        oled.setTextCursor(0,2);
        oled.printf("%s", men);
        oled.display();
        // impresion puerto serie
        serial.write(men, strlen(men));    

        ThisThread::sleep_for(tiempo_muestreo);

        oled.clearDisplay();
        oled.display();
        sprintf(men,"ErrorR.NTC: %d.%02d Celsius\n", entero_6,decimal_6);
        oled.setTextCursor(0,2);
        oled.printf("%s", men);
        oled.display();
        // impresion puerto serie
        serial.write(men, strlen(men));    

        ThisThread::sleep_for(tiempo_muestreo);

        oled.clearDisplay();
        oled.display();
        sprintf(men,"ErrorR.AHT: %d.%02d Celsius\n", entero_7,decimal_7);
        oled.setTextCursor(0,2);
        oled.printf("%s", men);
        oled.display();
        // impresion puerto serie
        serial.write(men, strlen(men));    

        ThisThread::sleep_for(1s);
    }
}

void condicion_start(void)
{
    sclk = 1;
    dio.output();
    dio = 1;
    ThisThread::sleep_for(1ms);
    dio = 0;
    sclk = 0;
}

void condicion_stop(void)
{
    sclk = 0;
    dio.output();
    dio = 0;
    ThisThread::sleep_for(1ms);
    sclk = 1;
    dio = 1;
}

void send_byte(char data)
{
    dio.output();
    for (int i = 0; i < 8; i++)
    {
        sclk = 0;
        dio = (data & 0x01) ? 1 : 0;
        data >>= 1;
        sclk = 1;
    }
    // Esperar el ACK
    dio.input();
    sclk = 0;
    ThisThread::sleep_for(1ms);
    // Esperar señal de ACK si es necesario
    if (dio == 0) 
    {
        sclk = 1;
        sclk = 0;
    }
}

void send_data(int number) {
    condicion_start();
    send_byte(escritura);
    condicion_stop();
    condicion_start();
    send_byte(dir_display);

    // Descomponer el número en dígitos y enviar al display
    int digit[4] = {0};
    for (int i = 0; i < 4; i++)
    {
        digit[i] = number % 10;
        number /= 10;
    }

    // Enviar los datos al display de derecha a izquierda
    for(int i = 3; i >= 0; i--) {
        send_byte(digitToSegment[digit[i]]);
    }
}
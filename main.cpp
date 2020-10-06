/*
 * Copyright (c) 2006-2020 Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mbed.h"
#include "EthernetInterface.h"

class API {
    private:
        SocketAddress addr;
        TCPSocket socket;
        EthernetInterface net;
        string hostname;
        
        void connect(){
            // Bring up the ethernet interface
            printf("Ethernet socket\n");
            this->net.connect();
            this->socket.open(&this->net);
            // Set target host by hostname and connect
            this->net.gethostbyname(this->hostname.c_str(), &this->addr);
            this->addr.set_port(80);
            this->socket.connect(this->addr);
        };
        void disconnect(){
            // Close the socket to return its memory and bring down the network interface
            this->socket.close();
            
            // Bring down the ethernet interface
            this->net.disconnect();
            printf("Done\r\n\r\n");
        };
        void displayIPAddress(){
            // Show the network address
            this->net.get_ip_address(&this->addr);
            printf("IP address: %s\n", this->addr.get_ip_address() ? this->addr.get_ip_address() : "None");
        }

    public:
        API(string hostname) : hostname(hostname) {}

        // Reference https://docs.postman-echo.com/#1eb1cf9d-2be7-4060-f554-73cd13940174
        // temperature_post("/post", "body:'this is a string body'")
        // temperature_post("/post", "{body:'this is a string body'}", "Content-Type:application/json")
        void http_post(string endpoint, string body, string options = ""){
            char sbuffer[255]; 
            char rbuffer[255];

            char header[100];

            sprintf(header, "Content-Type: application/json\r\nContent-length: %d", body.length());

            //connect
            this->connect();
            this->displayIPAddress();

            // Send a simple http post request
            sprintf(sbuffer, "POST %s HTTP/1.1\r\nHost: %s\r\n%s\r\n\r\n%s", endpoint.c_str(), this->hostname.c_str(), header, body.c_str());
            int scount = this->socket.send(sbuffer, sizeof sbuffer);
            printf("send %d [%.*s]\n%s\r\n", scount, strstr(sbuffer, "\r\n") - sbuffer, sbuffer, sbuffer);
            // printf("sent %s\r\n", sbuffer);
            
            // Recieve a simple http response and print out the response line
            int rcount = this->socket.recv(rbuffer, sizeof rbuffer);

            printf("recv %d [%.*s]\n%s\r\n", rcount, strstr(rbuffer, "\r\n") - rbuffer, rbuffer, rbuffer);
            // printf("recv %s\r\n", rbuffer);

            //disconnect
            this->disconnect();
        }
};

class Temperature {
    private:
        AnalogIn analog;
        // Specific variables the F746NG TemperatureSensor
        unsigned int temperature_analog, beta = 4275; //4275 3975
        float temperature, resistance;
        // Custom variable to track temperature scale
        bool scale;

        float calculate_kelvin(){
            /* Read analog value */
            this->temperature_analog = this->analog.read_u16(); 
        
            /* Calculate the resistance of the thermistor from analog votage read. */
            this->resistance= (float) 10000.0 * (65536.0 / this->temperature_analog);
        
            /* Convert the resistance to kelvin using Steinhart's Hart equation */
            this->temperature = 1 / ((log(this->resistance/10000.0) / this->beta) + (1.0 / 298.15));
            return this->temperature;
        }

    public:
        Temperature(AnalogIn analog) : analog(analog) {}

        // Convert Kelvin into celsius, and set the temperature scale to celsius
        float read_celsius(){
           this->scale = 0;
           return this->calculate_kelvin() - 273.15;
        }

        // Convert Kelvin into fahrenheit, and set the temperature scale to fahrenheit
        float read_fahrenhiet(){
           this->scale = 1;
           return this->calculate_kelvin() * 9/5 - 459.67; 
        }

        // Return temperature scale
        string read_temperature_scale(){
            return this->scale ? "Fahrenheit" : "Celsius";
        }
};

int main()
{
    API api("10.130.54.61");
    Temperature analog(A1);

    //<ctime>
    time_t rawtime;
    struct tm * timeinfo;
    char buffer [80];

    //Listen for readings
    while(1) {
        //get time
        time (&rawtime);
        timeinfo = localtime (&rawtime);
        char body[255];
        sprintf(body, "{\"temperature\":\"%2.2f\", \"scale\":\"%s\"}", analog.read_celsius(), analog.read_temperature_scale().c_str());
        api.http_post("/insert.php", body);
        
        ThisThread::sleep_for(60s);
    }
}
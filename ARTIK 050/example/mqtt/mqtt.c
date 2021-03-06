#include "wiced.h"
#include "mqtt_api.h"
#include "command_console.h"
#include "command_console_wifi.h"

#define MQTT_CONSOLE_COMMAND_HISTORY_LENGTH  (10)
#define MAX_MQTT_COMMAND_LENGTH              (100)
#define WICED_MQTT_DELAY_IN_MILLISECONDS     (50)


/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/



/******************************************************
 *               Function Declarations
 ******************************************************/
static wiced_result_t mqtt_connection_event_cb( wiced_mqtt_object_t mqtt_object, wiced_mqtt_event_info_t *event );

static int                     init                ( int argc, char *argv[] );
static int                     deinit                ( int argc, char *argv[] );
static int                     join_wifi                  ( int argc, char *argv[] );
static int                     connect               ( int argc, char *argv[] );
static int                     disconnect            ( int argc, char *argv[] );
static int                     subscribe                        ( int argc, char *argv[] );
static int                     unsubscribe                      ( int argc, char *argv[] );
static int                     publish                        ( int argc, char *argv[] );



/******************************************************
 *                    Constants
 ******************************************************/


#define MQTT_CONSOLE_COMMANDS \
    { (char*) "mqtt_init",       init,       0,      NULL, NULL, (char *)"",                                             (char *)"initialize the mqtt" }, \
    { (char*) "mqtt_deinit",     deinit,     0,      NULL, NULL, (char *)"",                                             (char *)"deinitialize the mqtt" }, \
    { (char*) "mqtt_connect",    connect,    2,      NULL, NULL, (char *)"<broker_ip>  <mqtt client-id>",                (char *)"connect to mqtt broker server" }, \
    { (char*) "mqtt_disconnect", disconnect, 0,      NULL, NULL, (char *)"",                                             (char *)"disconnect from mqtt broker server" }, \
    { (char*) "wifi_join",       join_wifi,  4,      NULL, NULL, (char *)"<ssid> <open|wep|wpa_aes|wpa_tkip|wpa2|wpa2_tkip> <security-key>  <band[0 = Auto | 1 = 5GHz | 2 = 2.4GHz]>",            (char *)"join to a wifi access point with preferred radio band" }, \
    { (char*) "mqtt_publish",    publish,    3,      NULL, NULL, (char *)"<qos> <topic> <payload>",                      (char *)"publish a message" }, \
    { (char*) "mqtt_subscribe",  subscribe,  2,      NULL, NULL, (char *)"<topic> <qos>",                                (char *)"subscribe to a topic" }, \
    { (char*) "mqtt_unsubscribe",unsubscribe,1,      NULL, NULL, (char *)"<topic>",                                      (char *)"unsubscribe from a  topic" }, \

static char                     mqtt_command_buffer[MAX_MQTT_COMMAND_LENGTH];
static char                     mqtt_command_history_buffer[MAX_MQTT_COMMAND_LENGTH * MQTT_CONSOLE_COMMAND_HISTORY_LENGTH];

/******************************************************
 *                    Structures
 ******************************************************/

const command_t mqtt_console_command_table[] =
{
    MQTT_CONSOLE_COMMANDS
    CMD_TABLE_END
};

static wiced_mqtt_callback_t callbacks = mqtt_connection_event_cb;

static wiced_semaphore_t semaphore;
static wiced_mqtt_object_t mqtt_object;
static wiced_mqtt_event_type_t expected_event;



/******************************************************
 *               Function Definitions
 ******************************************************/

static const wiced_ip_setting_t device_init_ip_settings =
{
    INITIALISER_IPV4_ADDRESS( .ip_address, MAKE_IPV4_ADDRESS(192,168,0,3) ),
    INITIALISER_IPV4_ADDRESS( .netmask,    MAKE_IPV4_ADDRESS(255,255,255,  0) ),
    INITIALISER_IPV4_ADDRESS( .gateway,    MAKE_IPV4_ADDRESS(192,168,  0,  1) ),
};
void application_start( void )
{
	 wiced_interface_t interface;
      wiced_result_t result;

      wiced_init( );

      WPRINT_APP_INFO( ( "MQTT console start\n") );

      result = wiced_network_up_default( &interface, &device_init_ip_settings );
      while(result!= WICED_SUCCESS){
    	  result = wiced_network_up_default( &interface, &device_init_ip_settings );
      }
      result = command_console_init(STDIO_UART, sizeof(mqtt_command_buffer), mqtt_command_buffer,
                                          MQTT_CONSOLE_COMMAND_HISTORY_LENGTH, mqtt_command_history_buffer, " ");

      if (result != WICED_SUCCESS)
      {
              WPRINT_APP_INFO(("ERROR: Starting the command console\r\n"));
      }
      console_add_cmd_table( mqtt_console_command_table );
}

/******************************************************
 *               Static Function Definitions
 ******************************************************/


/*
 * Call back function to handle connection events.
 */
static wiced_result_t mqtt_connection_event_cb( wiced_mqtt_object_t mqtt_object, wiced_mqtt_event_info_t *event )
{

    switch ( event->type )
    {
        case WICED_MQTT_EVENT_TYPE_CONNECT_REQ_STATUS:
        case WICED_MQTT_EVENT_TYPE_DISCONNECTED:
        case WICED_MQTT_EVENT_TYPE_PUBLISHED:
        case WICED_MQTT_EVENT_TYPE_SUBCRIBED:
        case WICED_MQTT_EVENT_TYPE_UNSUBSCRIBED:
        {
            if (event->type == WICED_MQTT_EVENT_TYPE_PUBLISHED)
            {
                WPRINT_APP_INFO(( "MESSAGE [ID: %u] published\n\n",event->data.msgid) );
            }
            expected_event = event->type;
            wiced_rtos_set_semaphore( &semaphore );
        }
            break;
        case WICED_MQTT_EVENT_TYPE_PUBLISH_MSG_RECEIVED:
        {
            wiced_mqtt_topic_msg_t msg = event->data.pub_recvd;
            WPRINT_APP_INFO(( "[MQTT] Received %.*s  for TOPIC : %.*s\n\n", (int) msg.data_len, msg.data, (int) msg.topic_len, msg.topic ));
        }
            break;
        default:
            break;
    }
    return WICED_SUCCESS;
}

/*
 * Creating and initialize mqtt object
 */

static int  init( int argc, char *argv[] )
{
    if(argc > 1)
       {
            WPRINT_APP_INFO(( "ERROR: Too many arguments \n" ));
            return WICED_ERROR;
       }

    /* Memory allocated for mqtt object*/
    mqtt_object = (wiced_mqtt_object_t) malloc( WICED_MQTT_OBJECT_MEMORY_SIZE_REQUIREMENT );
    if ( mqtt_object == NULL )
     {
         WPRINT_APP_ERROR(("ERROE: Don't have memory to allocate for mqtt object...\n"));
         return WICED_ERROR;
     }

     wiced_mqtt_init( mqtt_object );
     return WICED_SUCCESS;

}

/*
 *  Removing  mqtt object
 */

static int   deinit( int argc, char *argv[] )
{
    wiced_mqtt_deinit( mqtt_object );
    if(argc > 1)
       {
            WPRINT_APP_INFO(( "ERROR: Too many arguments \n" ));
            return WICED_ERROR;
       }

    free( mqtt_object );
    mqtt_object = NULL;
    return WICED_SUCCESS;
}


/*
 *  Join to a WiFi Access Point
 */

static int join_wifi(int argc,char *argv[])
{
   int32_t band = 0;
   if(argc > 5)
   {
       WPRINT_APP_INFO(( "ERROR: Too many arguments \n" ));
       return WICED_ERROR;
   }

   band = atoi( argv[4] );

   if ( ( band ) >= 0 && ( band <= WLC_BAND_2G ) )
   {
           if ( wwd_wifi_set_preferred_association_band( band ) != WWD_SUCCESS )
           {
               WPRINT_APP_INFO( ("Failed to set preferred band for association.\n") );
           }
    }


   if( join(argc-1,argv) != WICED_SUCCESS)
    {
       WPRINT_APP_INFO(( "ERROR: WIFI N/W  is not joined \n" ));
       return WICED_ERROR;
    }

    return WICED_SUCCESS;
}



/*
 *  Connect to MQTT Broker
 */

static int connect(int argc,char *argv[])
{
    wiced_ip_address_t ip;
    wiced_mqtt_pkt_connect_t conninfo;

    if(argc > 3)
     {
         WPRINT_APP_INFO(( "ERROR: Too many arguments \n" ));
         return WICED_ERROR;
     }

    memset( &conninfo, 0, sizeof( conninfo ) );
    str_to_ip(argv[1],&ip);

    conninfo.port_number = 8883;                   /* set to 0 indicates library to use default settings */
    conninfo.mqtt_version = WICED_MQTT_PROTOCOL_VER4;
    conninfo.clean_session = 1;
    conninfo.client_id = (uint8_t*)argv[2];
    conninfo.keep_alive = 10;
    conninfo.username = "DEVICE TOKEN";
    conninfo.password = "DEVICE ID";
    conninfo.peer_cn = NULL;

    if ( wiced_mqtt_connect( mqtt_object, &ip, WICED_STA_INTERFACE, callbacks, NULL, WICED_TRUE, &conninfo )!= WICED_SUCCESS)
    {
        WPRINT_APP_INFO(( "ERROR: Broker is not connected \n" ));
        return WICED_ERROR;
    }

     return WICED_SUCCESS;
}

/*
 *  Disconnect from MQTT Broker
 */


static int disconnect(int argc,char *argv[])
{

    if(argc > 1)
    {
        WPRINT_APP_INFO(( "ERROR: Too many arguments \n" ));
         return WICED_ERROR;
    }
    if ( wiced_mqtt_disconnect( mqtt_object ) != WICED_SUCCESS )
    {
        WPRINT_APP_INFO(( "ERROR: Broker is not disconnected \n" ));
        return WICED_ERROR;
    }

   return WICED_SUCCESS;

}

/*
 *  Subscribe to a topic
 */

static int subscribe(int argc,char *argv[])
{
    wiced_mqtt_msgid_t pktid;
    if(argc > 3)
    {
        WPRINT_APP_INFO(( "ERROR: Too many arguments \n" ));
         return WICED_ERROR;
    }

   pktid = wiced_mqtt_subscribe( mqtt_object, argv[1], atoi( argv[2]) );
   if ( pktid == 0 )
   {
       WPRINT_APP_INFO(( "ERROR: Topic  is  not subscribed \n" ));
       return WICED_ERROR;
   }
   wiced_rtos_delay_milliseconds( WICED_MQTT_DELAY_IN_MILLISECONDS  );

    return WICED_SUCCESS;
}

/*
 * Unsubscribe from a topic
 */

static int unsubscribe(int argc,char *argv[])
{
    wiced_mqtt_msgid_t pktid;
    if(argc > 2)
    {
        WPRINT_APP_INFO(( "ERROR: Too many arguments \n" ));
         return WICED_ERROR;
    }

    pktid = wiced_mqtt_unsubscribe( mqtt_object, argv[1] );

    if ( pktid == 0 )
    {
        WPRINT_APP_INFO(( "ERROR! Topic  is not unsubscribed \n" ));
        return WICED_ERROR;
    }

    return WICED_SUCCESS;
}

/*
 * Publish (send) a message
 */

static int publish(int argc,char *argv[])
{
    wiced_mqtt_msgid_t pktid;
    if(argc > 4)
    {
        WPRINT_APP_INFO(( "ERROR: Too many arguments \n" ));
         return WICED_ERROR;
    }
    pktid = wiced_mqtt_publish ( mqtt_object, (uint8_t*)argv[2], (uint8_t*)argv[3], strlen(argv[3]), atoi(argv[1]) );
    if(  pktid == 0 )
    {
       WPRINT_APP_INFO(( "ERROR! Message is not published\n" ));
       return WICED_ERROR;
    }

    wiced_rtos_delay_milliseconds( WICED_MQTT_DELAY_IN_MILLISECONDS  );
    return WICED_SUCCESS;
}


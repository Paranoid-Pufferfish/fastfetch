#include "bluetooth.h"

#ifdef FF_HAVE_DBUS
#include "common/dbus.h"

/* Example dbus reply, striped to only the relevant parts:
array [                                                     //root
    dict entry(                                             //object
        object path "/org/bluez/hci0/dev_03_21_8B_91_16_4D"
        array [
            dict entry(                                     //property
                string "org.bluez.Device1"
                array [
                    dict entry(
                        string "Address"
                        variant string "03:21:8B:91:16:4D"
                    )
                    dict entry(
                        string "Name"
                        variant string "JBL TUNE160BT"
                    )
                    dict entry(
                        string "Connected"
                        variant boolean true
                    )
                ]
            )
            dict entry(                                     //property
                string "org.bluez.Battery1"
                array [
                    dict entry(
                        string "Percentage"
                        variant byte 100
                    )
                ]
            )
        ]
    )
]
*/

static void detectBluetoothProperty(FFBluetoothResult* bluetooth, FFDBusData* dbus, DBusMessageIter* iter, bool* connected)
{
    if(dbus->lib->ffdbus_message_iter_get_arg_type(iter) != DBUS_TYPE_DICT_ENTRY)
        return;

    DBusMessageIter dictIter;
    dbus->lib->ffdbus_message_iter_recurse(iter, &dictIter);

    if(dbus->lib->ffdbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_STRING)
        return;

    const char* propertyType;
    dbus->lib->ffdbus_message_iter_get_basic(&dictIter, &propertyType);

    if(strstr(propertyType, "Device") == NULL && strstr(propertyType, "Battery") == NULL)
        return; //We don't care about other properties

    dbus->lib->ffdbus_message_iter_next(&dictIter);

    if(dbus->lib->ffdbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_ARRAY)
        return;

    DBusMessageIter arrayIter;
    dbus->lib->ffdbus_message_iter_recurse(&dictIter, &arrayIter);

    while(true)
    {
        if(dbus->lib->ffdbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_DICT_ENTRY)
            FF_DBUS_ITER_CONTINUE(dbus, &arrayIter)

        DBusMessageIter deviceIter;
        dbus->lib->ffdbus_message_iter_recurse(&arrayIter, &deviceIter);

        if(dbus->lib->ffdbus_message_iter_get_arg_type(&deviceIter) != DBUS_TYPE_STRING)
            FF_DBUS_ITER_CONTINUE(dbus, &arrayIter)

        const char* deviceProperty;
        dbus->lib->ffdbus_message_iter_get_basic(&deviceIter, &deviceProperty);

        dbus->lib->ffdbus_message_iter_next(&deviceIter);

        if(strcmp(deviceProperty, "Address") == 0)
            ffDBusGetValue(dbus, &deviceIter, &bluetooth->address);
        else if(strcmp(deviceProperty, "Name") == 0)
            ffDBusGetValue(dbus, &deviceIter, &bluetooth->name);
        else if(strcmp(deviceProperty, "Icon") == 0)
            ffDBusGetValue(dbus, &deviceIter, &bluetooth->type);
        else if(strcmp(deviceProperty, "Percentage") == 0)
            ffDBusGetByte(dbus, &deviceIter, &bluetooth->battery);
        else if(strcmp(deviceProperty, "Connected") == 0)
        {
            ffDBusGetBool(dbus, &deviceIter, connected);
            if(!*connected)
                return; //Just for performance, we don't need to continue if we're not connected
        }

        FF_DBUS_ITER_CONTINUE(dbus, &arrayIter);
    }
}

static bool detectBluetoothObject(FFBluetoothResult* bluetooth, FFDBusData* dbus, DBusMessageIter* iter)
{
    if(dbus->lib->ffdbus_message_iter_get_arg_type(iter) != DBUS_TYPE_DICT_ENTRY)
        return false;

    DBusMessageIter dictIter;
    dbus->lib->ffdbus_message_iter_recurse(iter, &dictIter);

    if(dbus->lib->ffdbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_OBJECT_PATH)
        return false;

    const char* objectPath;
    dbus->lib->ffdbus_message_iter_get_basic(&dictIter, &objectPath);

    //We don't want adapter objects
    if(strstr(objectPath, "dev_") == NULL)
        return false;

    dbus->lib->ffdbus_message_iter_next(&dictIter);

    if(dbus->lib->ffdbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_ARRAY)
        return false;

    DBusMessageIter arrayIter;
    dbus->lib->ffdbus_message_iter_recurse(&dictIter, &arrayIter);

    bool connected = false;

    while(true)
    {
        detectBluetoothProperty(bluetooth, dbus, &arrayIter, &connected);
        FF_DBUS_ITER_CONTINUE(dbus, &arrayIter);
    }

    if(!connected || bluetooth->name.length == 0)
    {
        ffStrbufClear(&bluetooth->address);
        ffStrbufClear(&bluetooth->name);
        ffStrbufClear(&bluetooth->type);
        bluetooth->battery = 0;
        return false;
    }

    return true;
}

static void detectBluetoothRoot(FFBluetoothResult* bluetooth, FFDBusData* dbus, DBusMessageIter* iter)
{
    if(dbus->lib->ffdbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
        return;

    DBusMessageIter arrayIter;
    dbus->lib->ffdbus_message_iter_recurse(iter, &arrayIter);

    while(true)
    {
        if(detectBluetoothObject(bluetooth, dbus, &arrayIter))
            return;

        FF_DBUS_ITER_CONTINUE(dbus, &arrayIter);
    }

    return;
}

static const char* detectBluetooth(const FFinstance* instance, FFBluetoothResult* bluetooth)
{
    FFDBusData dbus;
    const char* error = ffDBusLoadData(instance, DBUS_BUS_SYSTEM, &dbus);
    if(error)
        return error;

    DBusMessage* managedObjects = ffDBusGetMethodReply(&dbus, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if(!managedObjects)
        return "Failed to call GetManagedObjects";

    DBusMessageIter rootIter;
    if(!dbus.lib->ffdbus_message_iter_init(managedObjects, &rootIter))
    {
        dbus.lib->ffdbus_message_unref(managedObjects);
        return "Failed to get root iterator of GetManagedObjects";
    }

    detectBluetoothRoot(bluetooth, &dbus, &rootIter);

    dbus.lib->ffdbus_message_unref(managedObjects);
    return NULL;
}

#endif

void ffDetectBluetoothImpl(const FFinstance* instance, FFBluetoothResult* bluetooth)
{
    #ifdef FF_HAVE_DBUS
        ffStrbufAppendS(&bluetooth->error, detectBluetooth(instance, bluetooth));
    #else
        ffStrbufAppendS(&bluetooth->error, "Fastfetch was compiled without DBus support");
    #endif
}

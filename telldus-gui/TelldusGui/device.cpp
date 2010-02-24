#include "device.h"
#include <stdlib.h>

QHash<int, Device *> Device::devices;

int Device::callbackId = tdRegisterDeviceEvent( reinterpret_cast<TDDeviceEvent>(&Device::deviceEvent), 0);
int Device::deviceChangeCallbackId = tdRegisterDeviceChangeEvent( reinterpret_cast<TDDeviceChangeEvent>(&Device::deviceChangeEvent), 0);

const int SUPPORTED_METHODS = TELLSTICK_TURNON | TELLSTICK_TURNOFF | TELLSTICK_BELL | TELLSTICK_DIM | TELLSTICK_LEARN;

class DevicePrivate {
public:
	int id, state;
	QString name, protocol, model, stateValue;
	bool modelChanged, nameChanged, protocolChanged;
	mutable int methods;
	mutable QHash<QString, QString> settings;
};

Device::Device(int id)
{
	d = new DevicePrivate;
	d->id = id;
	d->model = "";
	d->state = 0;
	d->name = "";
	d->protocol = "";
	d->modelChanged = false;
	d->nameChanged = false;
	d->protocolChanged = false;
	d->methods = 0;

	if (d->id > 0) {
		char *name = tdGetName(id);
		d->name = QString::fromLocal8Bit( name );
		tdReleaseString( name );

		d->model = tdGetModel(id);

		char *protocol = tdGetProtocol(id);
		d->protocol = QString::fromLocal8Bit( protocol );
		tdReleaseString( protocol );

		updateState();
	}
	
	connect(this, SIGNAL(deviceChanged(int,int,int)), this, SLOT(deviceChangedSlot(int,int,int)));
}

Device::~Device() {
	delete d;
}

int Device::deviceId() const {
	return d->id;
}

void Device::setModel( const QString &model ) {
	d->model = model;
	d->modelChanged = true;
}

QString Device::model() const {
	return d->model;
}

void Device::setName( const QString & name ) {
	if (name.compare(d->name, Qt::CaseSensitive) == 0) {
		return;
	}
	d->name = name;
	d->nameChanged = true;
}

QString &Device::name() const {
	return d->name;
}

void Device::setParameter( const QString &name, const QString &value ) {
	d->settings[name] = value;
}

QString Device::parameter( const QString &name, const QString &defaultValue ) const {
	if (!d->settings.contains(name)) {
		char *p = tdGetDeviceParameter(d->id, name.toLocal8Bit(), defaultValue.toLocal8Bit());
		d->settings[name] = p;
		tdReleaseString(p);
	}
	return d->settings[name];
}

void Device::setProtocol( const QString & protocol ) {
	if (protocol.compare(d->protocol, Qt::CaseSensitive) == 0) {
		return;
	}
	d->protocol = protocol;
	d->protocolChanged = true;
}

QString &Device::protocol() const {
	return d->protocol;
}

int Device::methods() const {
	if (d->methods == 0) {
		const_cast<Device*>(this)->updateMethods();
	}
	return d->methods;
}

int Device::deviceType() const {
	return tdGetDeviceType(d->id);
}

Device *Device::getDevice( int id ) {

	if (devices.contains(id)) {
		return devices[id];
	}
	Device *device = new Device(id);
	devices[id] = device;
	return device;
}

Device *Device::newDevice( ) {
	return new Device(0);
}

bool Device::deviceLoaded( int id ) {
	return devices.contains(id);
}

void Device::save() {
	bool deviceIsAdded = false, methodsChanged = false;
	if (d->id == 0) { //This is a new device
		d->id = tdAddDevice();
		deviceIsAdded = true;
	}

	if (d->nameChanged || deviceIsAdded) {
		tdSetName(d->id, d->name.toLocal8Bit());
		d->nameChanged = false;
	}

	if (d->modelChanged || deviceIsAdded) {
		tdSetModel(d->id, d->model.toLocal8Bit());
		methodsChanged = true;
		d->modelChanged = false;
	}

	if (d->protocolChanged || deviceIsAdded) {
		tdSetProtocol(d->id, d->protocol.toLocal8Bit());
		methodsChanged = true;
		d->protocolChanged = false;
	}

	//Save all parameters
	for( QHash<QString, QString>::const_iterator it = d->settings.begin(); it != d->settings.end(); ++it) {
		QByteArray name(it.key().toLocal8Bit() );
		QByteArray value(it.value().toLocal8Bit() );
		tdSetDeviceParameter(d->id, name.constData(), value.constData());
	}

	if (methodsChanged) {
		updateMethods();
	}

	if (deviceIsAdded) {
		emit deviceAdded(d->id);
	}
}

void Device::turnOff() {
	triggerEvent( tdTurnOff( d->id ) );
}

void Device::turnOn() {
	triggerEvent( tdTurnOn( d->id ) );
}

void Device::bell() {
	triggerEvent( tdBell( d->id ) );
}

void Device::learn() {
	triggerEvent( tdLearn( d->id ) );
}

int Device::lastSentCommand() const {
	return d->state;
}

QString Device::lastSentValue() const {
	return d->stateValue;
}

void Device::updateMethods() {
	int methods = tdMethods(d->id, SUPPORTED_METHODS);
	if (d->methods != methods) {
		bool doEmit = (d->methods > 0);
		d->methods = methods;
		if (doEmit) {
			emit methodsChanged( d->methods );
		}
	}
}

void Device::updateState() {
	int lastSentCommand = tdLastSentCommand( d->id, SUPPORTED_METHODS );
	char *value = tdLastSentValue( d->id );
	QString stateValue = QString::fromLocal8Bit( value );
	tdReleaseString(value);
	
	if (lastSentCommand != d->state || stateValue != d->stateValue) {
		d->state = lastSentCommand;
		d->stateValue = stateValue;
		emit stateChanged(d->id, d->state);
	}
}

void Device::deviceChangedSlot(int deviceId, int eventId, int changeType) {
	if (eventId != TELLSTICK_DEVICE_CHANGED) {
		return;
	}
	switch( changeType ) {
		case TELLSTICK_CHANGE_NAME:
			if (d->nameChanged) {
				break;
			}
			char *name = tdGetName(d->id);
			d->name = QString::fromLocal8Bit( name );
			tdReleaseString( name );
			break;			
	}
}

void Device::triggerEvent( int messageId ) {
	if (messageId == TELLSTICK_SUCCESS) {
		//Update the last sent command
		updateState();
	} else {
		char *message = tdGetErrorString( messageId );
		emit showMessage( "", message, "" );
		tdReleaseString( message );
	}
}

void WINAPI Device::deviceEvent(int deviceId, int, const char *, int, void *) {
	if (Device::deviceLoaded( deviceId )) {
		Device *device = Device::getDevice( deviceId );
		if (device) {
			device->updateState();
		}
	}
}

void WINAPI Device::deviceChangeEvent(int deviceId, int eventId, int changeType, int, void *) {
	if (Device::deviceLoaded( deviceId )) {
		Device *device = Device::getDevice( deviceId );
		if (device) {
			emit device->deviceChanged(deviceId, eventId, changeType);
		}
	}
}


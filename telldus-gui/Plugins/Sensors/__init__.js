/** Sensors **/
__setupPackage__( __extension__ );

__postInit__ = function() {
	application.allDoneLoading.connect( com.telldus.sensors.init );
}

com.telldus.sensors = function() {
	var sensorList;
	function init() {
		var sensorData = 0;
		sensorList = loadSensorModel();
		sensorList.rowsRemoved.connect(function(){saveSensorModel();});
		sensorList.rowsInserted.connect(function(){saveSensorModel();});

		while(sensorData = com.telldus.core.sensor()) {
			var p = sensorData["protocol"];
			var m = sensorData["model"];
			var id = sensorData["sensorId"];
			var types = sensorData["dataTypes"];

			var tryFetchValue = function(p, m, id, types, type) {
				if (types & type) {
					sensorValue = com.telldus.core.sensorValue(p, m, id, type);
					sensorEvent(p, m, id, type, sensorValue["value"], sensorValue["timestamp"], true);
				}
			}
			tryFetchValue(p, m, id, types, com.telldus.core.TELLSTICK_TEMPERATURE);
			tryFetchValue(p, m, id, types, com.telldus.core.TELLSTICK_HUMIDITY);

		}

		com.telldus.core.sensorEvent.connect(sensorEvent);
		view = new com.telldus.qml.view({
			deleteSensor: deleteSensor
		});

		view.setProperty('sensorModel', sensorList);
		var initialViewMode = 'EDIT';
		for (var i=0; i < sensorList.length; ++i) {
			if (sensorList.get(i).showInList || sensorList.get(i).name !== "") {
				initialViewMode = 'VIEW';
				break;
			}
		}
		view.setProperty('initialViewMode', initialViewMode);
		saveSensorModel();
		view.load("main.qml");
		application.addWidget("sensors.gui", "icon.png", view);
	}

	function createSensor(protocol, model, id, name, showInList){
		var sensor = new com.telldus.sensors.sensor();
		sensor.protocol = protocol;
		sensor.model = model;
		sensor.id = id;
		sensor.name = name;
		sensor.nameChanged.connect(function() { saveSensorModel(); });
		sensor.showInList = showInList;
		sensor.showInListChanged.connect(function() { saveSensorModel(); });
		return sensor;
	}

	function deleteSensor(protocol, model, id){
		var i = 0;
		var found = false;
		for (; i < sensorList.length; ++i) {
			if (sensorList.get(i).protocol != protocol) {
				continue;
			}
			if (sensorList.get(i).model != model) {
				continue;
			}
			if (sensorList.get(i).id != id) {
				continue;
			}
			found = true;
			break;
		}
		if(found){
			sensorList.removeLater(i);
		}
	}

	function loadSensorModel(){
		var settings = new com.telldus.settings();
		var sensors = new com.telldus.qml.array();

		var sensorProperties = settings.value("sensors", "");
		if(sensorProperties){
			for (var i = 0; i < sensorProperties.length; i++) {
				var sensor = createSensor(sensorProperties[i].protocol, sensorProperties[i].model, sensorProperties[i].id, sensorProperties[i].name, sensorProperties[i].showInList=="true");
				for (var j = 0; j < sensorProperties[i].values.length; j++) {
					sensor.setValue(sensorProperties[i].values[j].type, sensorProperties[i].values[j].value, sensorProperties[i].values[j].lastUpdated)
				}
				sensors.push(sensor);
			}
		}
		return sensors;
	}

	function saveSensorModel(){
		var settings = new com.telldus.settings();
		var sensorProperties = new Array();

		for (var i = 0; i < sensorList.length; ++i) {
			var sensor = sensorList.get(i);

			var allValues = new Array();
			if(sensor.hasHumidity){
				var sensorValue = sensor.sensorValue(com.telldus.core.TELLSTICK_HUMIDITY);
				var value = {type: com.telldus.core.TELLSTICK_HUMIDITY, lastUpdated: sensorValue.lastUpdated, value: sensorValue.value};
				allValues.push(value);
			}
			if(sensor.hasTemperature){
				var sensorValue = sensor.sensorValue(com.telldus.core.TELLSTICK_TEMPERATURE);
				var value = {type: com.telldus.core.TELLSTICK_TEMPERATURE, lastUpdated: sensorValue.lastUpdated, value: sensorValue.value};
				allValues.push(value);
			}

			var sensorProp = {protocol:sensor.protocol, model:sensor.model, id:sensor.id, values:allValues, name:sensor.name, showInList:sensor.showInList};
			sensorProperties.push(sensorProp);
		}

		settings.setValue("sensors", sensorProperties);
	}

	function sensorEvent(protocol, model, id, dataType, value, timestamp, avoidSave) {

		var sensor = 0;
		for (var i = 0; i < sensorList.length; ++i) {
			if (sensorList.get(i).protocol != protocol) {
				continue;
			}
			if (sensorList.get(i).model != model) {
				continue;
			}
			if (sensorList.get(i).id != id) {
				continue;
			}
			sensor = sensorList.get(i);
			break;
		}

		if (!sensor) {
			sensor = createSensor(protocol, model, id, "", false);
			sensorList.push(sensor);
		}

		sensor.setValue(dataType, value, timestamp);

		if(!avoidSave){
			saveSensorModel();
		}
	}

	return { //Public functions
		init:init
	}

}();

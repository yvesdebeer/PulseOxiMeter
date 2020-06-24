//
//  ViewController.swift
//  PulseOximeter
//
//  Created by Yves Debeer on 02/06/2020.
//  Copyright © 2020 Yves Debeer. All rights reserved.
//

import UIKit
import CoreBluetooth
import CocoaMQTT

let oximeterServiceCBUUID = CBUUID(string: "cdeacb80-5235-4c07-8846-93a37ee6b86d")
let oximeterCharacteristicNotifyCBUUID = CBUUID(string: "cdeacb81-5235-4c07-8846-93a37ee6b86d")

class ViewController: UIViewController {
    
    @IBOutlet weak var spo2Label: UILabel!
    @IBOutlet weak var bpmLabel: UILabel!
    @IBOutlet weak var piLabel: UILabel!
    
    var centralManager: CBCentralManager!
    var oximeterPeripheral: CBPeripheral!
    
    var mqtt: CocoaMQTT!

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view.
        
        centralManager = CBCentralManager(delegate: self, queue: nil)
        setUpMQTT()
    }
    
    func setUpMQTT() {
        let clientID = "CocoaMQTT-" + String(ProcessInfo().processIdentifier)
        mqtt = CocoaMQTT(clientID: clientID, host: "test.mosquitto.org", port: 1883)
        //mqtt.username = ''
        //mqtt.password = ''
        mqtt.willMessage = CocoaMQTTWill(topic: "/will", message: "dieout")
        mqtt.keepAlive = 60
        mqtt.delegate = self
        mqtt.autoReconnect = true
        mqtt.logLevel = .debug
        mqtt.connect()
    }

}

extension ViewController: CocoaMQTTDelegate {
    func mqtt(_ mqtt: CocoaMQTT, didSubscribeTopic topics: [String]) {
        print("Subscribed")
        let message = "connected"
        mqtt.publish("oximeter/json", withString: message, qos: .qos1)
    }
    
    // These two methods are all we care about for now
    func mqtt(_ mqtt: CocoaMQTT, didConnect host: String, port: Int) {
    }
    
    func mqtt(_ mqtt: CocoaMQTT, didReceiveMessage message: CocoaMQTTMessage, id: UInt16 ) {
        print("Receving message")
    }
    
    // Other required methods for CocoaMQTTDelegate
    func mqtt(_ mqtt: CocoaMQTT, didReceive trust: SecTrust, completionHandler: @escaping (Bool) -> Void) {
        completionHandler(true)
    }
    
    func mqtt(_ mqtt: CocoaMQTT, didConnectAck ack: CocoaMQTTConnAck) {
        print("Connected")
        mqtt.subscribe("oximeter/json")
    }
    
    func mqtt(_ mqtt: CocoaMQTT, didPublishMessage message: CocoaMQTTMessage, id: UInt16) {
    }
    
    func mqtt(_ mqtt: CocoaMQTT, didPublishAck id: UInt16) {
    }
    
    func mqtt(_ mqtt: CocoaMQTT, didSubscribeTopic topic: String) {
    }
    
    func mqtt(_ mqtt: CocoaMQTT, didUnsubscribeTopic topic: String) {
    }
    
    func mqttDidPing(_ mqtt: CocoaMQTT) {
    }
    
    func mqttDidReceivePong(_ mqtt: CocoaMQTT) {
    }
    
    func mqttDidDisconnect(_ mqtt: CocoaMQTT, withError err: Error?) {
    }
    
    func _console(_ info: String) {
    }
}

extension ViewController: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
          case .unknown:
            print("central.state is .unknown")
          case .resetting:
            print("central.state is .resetting")
          case .unsupported:
            print("central.state is .unsupported")
          case .unauthorized:
            print("central.state is .unauthorized")
          case .poweredOff:
            print("central.state is .poweredOff")
          case .poweredOn:
            print("central.state is .poweredOn")
            centralManager.scanForPeripherals(withServices: [oximeterServiceCBUUID])
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        print(peripheral)
        oximeterPeripheral = peripheral
        oximeterPeripheral.delegate = self
        centralManager.stopScan()
        centralManager.connect(oximeterPeripheral)
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("Connected!")
        oximeterPeripheral.discoverServices([oximeterServiceCBUUID])
    }
    
}

extension ViewController: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
          print(service)
          peripheral.discoverCharacteristics(nil, for: service)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
      guard let characteristics = service.characteristics else { return }
      for characteristic in characteristics {
        // print(characteristic)
        if characteristic.properties.contains(.read) {
          print("\(characteristic.uuid): properties contains .read")
          peripheral.readValue(for: characteristic)
        }
        if characteristic.properties.contains(.notify) {
          print("\(characteristic.uuid): properties contains .notify")
          peripheral.setNotifyValue(true, for: characteristic)
        }
      }
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
      switch characteristic.uuid {
        case oximeterCharacteristicNotifyCBUUID:
          //print(characteristic.value ?? "no value")
          let oxivalues = oxireadings(from: characteristic)
          //print(oxivalues)
        default:
          print("Unhandled Characteristic UUID: \(characteristic.uuid)")
      }
    }
    private func oxireadings(from characteristic: CBCharacteristic) -> String {
      guard let characteristicData = characteristic.value else { return "Error" }
        let byte = characteristicData.first
        //print(characteristicData)
        if (byte == 0x81) {
            //print("Found 0x81")
            let bpm = characteristicData[1]
            //print(bpm)
            bpmLabel.text = String(bpm)
            let spo2 = characteristicData[2]
            //print(spo2)
            spo2Label.text = String(spo2)
            let pi:Float = Float(characteristicData[3])/10
            //print(pi)
            piLabel.text = String(pi)
            let message = "{\"d\":{\"bpm\":\(bpm),\"spo2\":\(spo2),\"pi\":\(String(pi))}}"
            if (bpm > 0 && bpm < 255) {
                mqtt.publish("oximeter/json", withString: message, qos: .qos1)
            }
        }
        return "Found"
    }
}

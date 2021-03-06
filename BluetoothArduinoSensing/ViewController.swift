//
//  ViewController.swift
//  ExampleRedBearChat
//
//  Created by Eric Larson on 9/26/17.
//  Copyright © 2017 Eric Larson. All rights reserved.
//

import UIKit
import Charts

class ViewController: UIViewController {
    
    // MARK: VC Properties
    lazy var bleShield = (UIApplication.shared.delegate as! AppDelegate).bleShield
    lazy var rssiTimer = Timer()
    lazy var valuesList = NSMutableArray()
    lazy var deltasList = NSMutableArray()
    
    @IBOutlet weak var spinner: UIActivityIndicatorView!
    @IBOutlet weak var brightnessLabel: UILabel!
    @IBOutlet var brightnessTextLabel: UILabel!
    @IBOutlet weak var ledLabel: UILabel!
    @IBOutlet weak var photoresLabel: UILabel!
    @IBOutlet var deviceNameLabel: UILabel!
    @IBOutlet var brightnessSlider: UISlider!
    @IBOutlet var photoresTextLabel: UILabel!
    @IBOutlet var prPollingRateSlider: UISlider!
    @IBOutlet weak var lineChart: LineChartView!
    
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        // BLE Connect Notification
        NotificationCenter.default.addObserver(self,
                                               selector: #selector(self.onBLEDidConnectNotification),
                                               name: NSNotification.Name(rawValue: kBleConnectNotification),
                                               object: nil)
        
        // BLE Disconnect Notification
        NotificationCenter.default.addObserver(self,
                                               selector: #selector(self.onBLEDidDisconnectNotification),
                                               name: NSNotification.Name(rawValue: kBleDisconnectNotification),
                                               object: nil)
        
        // BLE Recieve Data Notification
        NotificationCenter.default.addObserver(self,
                                               selector: #selector(self.onBLEDidRecieveDataNotification),
                                               name: NSNotification.Name(rawValue: kBleReceivedDataNotification),
                                               object: nil)
        self.spinner.startAnimating()
        
        self.lineChart.xAxis.axisMinimum = 0.0
        self.lineChart.xAxis.axisMaximum = 10000.0
        self.lineChart.xAxis.drawLabelsEnabled = false
        self.lineChart.rightAxis.drawLabelsEnabled = false
        self.lineChart.setScaleEnabled(true)
        self.lineChart?.chartDescription?.text = "Photoresistor Values"
        lineChartUpdate()
    }
    
    // Lesson Learned: Must remove observer, or else the BleReceivedDataNotification selector will be called
    // n times, where n is the number of times ViewController has disappeared and re-appeared
    override func viewDidDisappear(_ animated: Bool) {
        NotificationCenter.default.removeObserver(self,
                                                  name: NSNotification.Name(rawValue: kBleConnectNotification),
                                                  object: nil)
        NotificationCenter.default.removeObserver(self,
                                                  name: NSNotification.Name(rawValue: kBleDisconnectNotification),
                                                  object: nil)
        NotificationCenter.default.removeObserver(self,
                                                  name: NSNotification.Name(rawValue: kBleReceivedDataNotification),
                                                  object: nil)
    }
    
    func readRSSITimer(timer:Timer){
        bleShield.readRSSI { (number, error) in
            //update the RSSI if we want
        }
    }

    @objc func onBLEDidConnectNotification(notification:Notification){
        self.spinner.stopAnimating()
        print("Notification arrived that BLE Connected")
        if let ui = notification.userInfo{
            self.deviceNameLabel.text = ui["name"] as! String
        }
        // Schedule to read RSSI every 1 sec.
        rssiTimer = Timer.scheduledTimer(withTimeInterval: 1.0,
                                         repeats: true,
                                         block: self.readRSSITimer)
    }

    @objc func onBLEDidDisconnectNotification(notification:Notification){
        print("Notification arrived that BLE Disconnected a Peripheral")
    }

    @objc func onBLEDidRecieveDataNotification(notification:Notification){
        if let msg = notification.userInfo?["data"] as! Data?{
            if(msg[0] == 0x01){ //photoresistor value update
                let photoResistorRaw:uint = uint(msg[1]) << 8 + uint(msg[2])
                var photoResistorTimeDelta:uint = uint(msg[3])<<24
                photoResistorTimeDelta += uint(msg[4])<<16
                photoResistorTimeDelta += uint(msg[5])<<8
                photoResistorTimeDelta += uint(msg[6])
                if(photoResistorRaw > 3500){
                    self.view.backgroundColor = UIColor.white
                    self.deviceNameLabel.textColor = UIColor.black
                    self.brightnessLabel.textColor = UIColor.black
                    self.photoresLabel.textColor = UIColor.black
                    self.brightnessTextLabel.textColor = UIColor.black
                    self.photoresTextLabel.textColor = UIColor.black
                    
                }
                else{
                    self.view.backgroundColor = UIColor.black
                    self.deviceNameLabel.textColor = UIColor.white
                    self.brightnessLabel.textColor = UIColor.white
                    self.photoresLabel.textColor = UIColor.white
                    self.brightnessTextLabel.textColor = UIColor.white
                    self.photoresTextLabel.textColor = UIColor.white
                    
                }
                self.valuesList.add(photoResistorRaw)
                self.deltasList.add(photoResistorTimeDelta)
                self.lineChartUpdate()
            }
            else if(msg[0] == 0x02){ //Updated LED Value
                let ledValueRaw = msg[1]
                self.brightnessSlider.value = ((Float(ledValueRaw)/255.0)*100).rounded()
                self.brightnessLabel.text = String(Int(self.brightnessSlider.value)) + "%"

            }
            else{
                print("Unable to handle message with header: " + String(format: "%2X", msg[0]))
            }
        }
    }

    @IBAction func changeBrightness(_ sender: UISlider) {
        var instruct_arr = [uint_fast8_t]()
        instruct_arr.append(uint_fast8_t(1))
        instruct_arr.append(uint_fast8_t(sender.value * 2.55))
        let my_data = Data(instruct_arr)
        bleShield.write(my_data)
        self.brightnessLabel.text = String(Int(sender.value)) + "%"
    }

    @IBAction func changePhotoRes(_ sender: UISlider) {
        if(sender.value>=5){
            var instruct_arr = [uint_fast8_t]()
            instruct_arr.append(uint_fast8_t(0x03))
            instruct_arr.append(uint_fast8_t(sender.value))
            let my_data = Data(instruct_arr)
            bleShield.write(my_data)
            self.photoresLabel.text = String(Int(sender.value) * 10) + "ms"
        } else {
            var instruct_arr = [uint_fast8_t]()
            instruct_arr.append(uint_fast8_t(0x03))
            instruct_arr.append(uint_fast8_t(0))
            let my_data = Data(instruct_arr)
            bleShield.write(my_data)
            self.photoresLabel.text = "Off"
        }
    }

    @IBAction func pollLEDButtonPressed(_ sender: UIButton) {
        var instruct_arr = [uint_fast8_t]()
        instruct_arr.append(uint_fast8_t(0x02))
        let my_data = Data(instruct_arr)
        bleShield.write(my_data)
    }
    
    func lineChartUpdate(){
        var lineChartEntry = [ChartDataEntry]()
        let max = 10000
        var sumDeltas = 0
        for idx in (0 ..< self.valuesList.count).reversed(){
            sumDeltas += self.deltasList.object(at: idx) as! Int
            if(sumDeltas<max){
                print("sumdeltas: " + String(sumDeltas))
                lineChartEntry.append(ChartDataEntry(x: Double(max-sumDeltas), y: Double(self.valuesList.object(at: idx) as! Int)))
            }
        }
        let dataSet = LineChartDataSet(values: lineChartEntry.reversed(), label: nil)
        dataSet.colors = [NSUIColor.blue]
        dataSet.mode = LineChartDataSet.Mode.cubicBezier;
        dataSet.drawCircleHoleEnabled = false
        dataSet.circleRadius=2.0
        dataSet.drawValuesEnabled = false
        dataSet.circleColors = [NSUIColor.red]
        let data = LineChartData()
        data.addDataSet(dataSet)
        
        self.lineChart.data = data
        self.lineChart?.chartDescription?.text = "Photoresistor Values"
        self.lineChart.notifyDataSetChanged()
    }
    

}









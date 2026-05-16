require('dotenv').config()
const mongoose = require('mongoose')
const mqtt = require('mqtt')

const url = process.env.MONGODB_URI

mongoose.set('strictQuery', false)
mongoose.connect(url, { family: 4 })

const entrySchema = new mongoose.Schema({
  temperature: Number,
  humidity: Number,
  temp_limit: Number,
  hum_limit: Number,
  window: String,
  fan: String,
  timestamp: Date
})

const Entry = mongoose.model('Entry', entrySchema)

const client = mqtt.connect(process.env.MQTT_URL, {
  username: process.env.MQTT_USERNAME,
  password: process.env.MQTT_PASSWORD
})

let latest = null

client.on('connect', () => {
  client.subscribe('greenhouse/sensor1')
})

client.on('message', (topic, message) => {
  try {
    latest = JSON.parse(message.toString())
  } catch (err) {}
})

setInterval(async () => {
  if (!latest) return
  try {
    await Entry.create({
      ...latest,
      timestamp: new Date()
    })
  } catch (err) {}
}, 20 * 1000)     //10 * 60 * 1000 for every 10 mins
const mongoose = require('mongoose');

const DataSchema = new mongoose.Schema({
  temp: Number,
  hum: Number,
  message: String,
  mode: String
}, { timestamps: true });

module.exports = mongoose.model('Data', DataSchema);
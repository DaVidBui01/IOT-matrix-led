const Data = require('../models/Data');

exports.getAllData = async (req, res) => {
  const data = await Data.find().sort({ createdAt: -1 });
  res.json(data);
};

exports.insertData = async (req, res) => {
  const saved = await Data.create(req.body);
  res.json(saved);
};
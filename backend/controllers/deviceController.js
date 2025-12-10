exports.sendCommand = async (req, res) => {
  console.log("Command:", req.body);
  res.json({ ok: true });
};
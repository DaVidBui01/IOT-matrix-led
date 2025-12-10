const router = require('express').Router();
const { sendCommand } = require('../controllers/deviceController');

router.post('/command', sendCommand);

module.exports = router;
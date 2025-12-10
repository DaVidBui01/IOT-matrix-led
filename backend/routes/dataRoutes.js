const router = require('express').Router();
const { getAllData, insertData } = require('../controllers/dataController');

router.get('/', getAllData);
router.post('/', insertData);

module.exports = router;
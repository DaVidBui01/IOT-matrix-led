require("dotenv").config();
console.log("MONGO_URI from ENV =", process.env.MONGO_URI); // <--- thêm dòng này

const express = require("express");
const app = express();
const mongoose = require("mongoose");
const path = require("path");

app.use(express.json());
app.use(express.urlencoded({ extended: true }));

mongoose
  .connect(process.env.MONGO_URI)
  .then(() => console.log("MongoDB connected"))
  .catch((err) => console.error("MongoDB error:", err));

app.use("/api/data", require("./routes/dataRoutes"));
app.use("/api/device", require("./routes/deviceRoutes"));

module.exports = app;

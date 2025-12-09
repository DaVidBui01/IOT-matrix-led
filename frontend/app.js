const express = require("express");
const path = require("path");

const app = express();
app.set("view engine", "pug");
app.set("views", path.join(__dirname, "views"));

app.use(express.static(path.join(__dirname, "public")));
app.use(express.urlencoded({ extended: true }));
app.use(express.json());

// Routes
app.use("/", require("./routes/dashboard"));
app.use("/controller", require("./routes/controller"));
app.use("/reports", require("./routes/reports"));

module.exports = app;

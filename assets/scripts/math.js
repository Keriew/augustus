Math.clamp = function (a, b, c) {
    return Math.max(b, Math.min(c, a));
}

Math.randomIndex = function(min, max) {
  var offset = min;
  var range = (max - min) + 1;

  var randomNumber = Math.floor( Math.random() * range) + offset;
  return randomNumber;
}

Math.percentage = function(value, base) {
  return Math.floor(value / base * 100);
}

log_info("!!! Module Math Loaded")


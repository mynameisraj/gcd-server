var getEmail = function() {
  var domain, name, tld;
  name = "raj";
  domain = "mynameisraj";
  tld = "com";
  return "mailto:" + name + "@" + domain + "." + tld;
};

var init = function() {
  document.getElementById("email").setAttribute("href", getEmail());
};

window.addEventListener("DOMContentLoaded", init, false);

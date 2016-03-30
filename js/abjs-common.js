/**
 * @file
 * Adds common javascript functionality. Note that this file does not
 * stand alone, and is included as an inline script.
 */

var abCookies = {
  getCookie: function (sKey) {
    if (!sKey) { return null; }
    abKey = abjs.cookiePrefix + sKey;
    return decodeURIComponent(document.cookie.replace(new RegExp("(?:(?:^|.*;)\\s*" + encodeURIComponent(abKey).replace(/[\-\.\+\*]/g, "\\$&") + "\\s*\\=\\s*([^;]*).*$)|^.*$"), "$1")) || null;
  },
  setCookie: function (sKey, sValue) {
    abKey = abjs.cookiePrefix + sKey;
    document.cookie = encodeURIComponent(abKey) + "=" + encodeURIComponent(sValue) + "; max-age=" + abjs.cookieLifetime*24*60*60 + abjs.cookieDomain + "; path=/" + abjs.cookieSecure;
    return true;
  }
};
for(var i = abjs.tests.length - 1; i >= 0; i--){
  for(var j = 0; j < abjs.tests[i].conditions.length; j++){
    var funcString = abjs.tests[i].conditions[j];
    if(!abjs.tests[i].conditions[j]()) {
      break;
    }
  }
  if(j !== abjs.tests[i].conditions.length){
    abjs.tests.splice(i, 1);
  }
}
for(var i = 0; i < abjs.tests.length; i++){
  if(abCookies.getCookie(abjs.tests[i].name)){
    for(var j = 0; j < abjs.tests[i].experiences.length; j++){
      if(abCookies.getCookie(abjs.tests[i].name) === abjs.tests[i].experiences[j].name){
        abjs.tests[i].activeExperience = j;
        break;
      }
    }
  }
  else{
    var randomNum = Math.random();
    var fractionSum = 0;
    for(var j = 0; j < abjs.tests[i].experiences.length; j++){
      if(randomNum >= fractionSum && randomNum < fractionSum + abjs.tests[i].experiences[j].fraction){
        abCookies.setCookie(abjs.tests[i].name,abjs.tests[i].experiences[j].name);
        abjs.tests[i].activeExperience = j;
        break;
      }
      fractionSum += abjs.tests[i].experiences[j].fraction;
    }
  }
}
for(var i = 0; i <abjs.tests.length; i++){
  if(typeof abjs.tests[i].activeExperience !== 'undefined') {
    abjs.tests[i].experiences[abjs.tests[i].activeExperience].script();
  }
}
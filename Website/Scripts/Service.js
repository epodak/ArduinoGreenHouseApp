angular.module('greenhouse').
     service("greenApiService", ['$http', '$q', function ($http, $q) {

       var host = 'http://miticv.duckdns.org:8080';
       //var host = '';
       return ({

         isAdmin: isAdmin,

         setSettings: setSettings,
         getSettings: getSettings,

         getTime: getTime,
         setTime: setTime,

         doReboot: doReboot
       });
       //#endregion
       //#region isAdmin
       function isAdmin(pin) {

         return $http({
           method: "get",
           url: host + '/admin?U=' + pin
         });
       }
       //#endregion
       //#region doReboot
       function doReboot() {

         return $http({
           method: "put",
           headers: { 'PP': sessionStorage.getItem('pass') },
           url: host + '/reboot'
         });
       }
       //#endregion
       //#region setSettings
       function setSettings(settings) {

         return $http({
           method: "put",
           headers: { 'PP': sessionStorage.getItem('pass') },
           url: host + '/settings?U=' + settings
         });
       }
       //#endregion
       //#region getSettings
       function getSettings() {

         return $http({
           method: "get",
           headers: { 'PP': sessionStorage.getItem('pass') },
           url: host + '/file/SETTINGS.TXT'
         });
       }
       //#endregion
       //#region getTime
       function getTime() {

         return $http({
           method: "get",
           headers: { 'PP': sessionStorage.getItem('pass') },
           url: host + '/clock'
         });
       }
       //#endregion
       //#region setTime
       function setTime(t) {

         return $http({
           method: "put",
           headers: { 'PP': sessionStorage.getItem('pass') },
           url: host + '/clock?U=' + t
         });
       }
       //#endregion
     }]);


angular.module('greenhouse').
  value('toastr', toastr).
  value('window', window).
  factory('NotifierService',
  ['toastr', '$window', '$log',
  function NotifierService(toastr, $window, $log) {
    'use strict';

    toastr.options.timeOut = 5000;
    toastr.options.positionClass = 'toast-bottom-right';


    function notify(type) {
      return function (message, title) {
        toastr[type](message, title);
        //$log.debug(type + ':', message);
      };
    }

    return {
      error: notify('error'),
      info: notify('info'),
      success: notify('success'),
      warning: notify('warning')
    };
  }]);
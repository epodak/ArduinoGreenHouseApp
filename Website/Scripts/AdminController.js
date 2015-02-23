
angular.module('greenhouse').
  controller('adminController', ['$scope', 'greenApiService', 'NotifierService', '$q', '$interval', '$location',
                        function ($scope, greenApiService, NotifierService, $q, $interval, $location) {

                          //$scope.settings = ''; //settings received from the device
                          $scope.set = {  //settings received from the device
                            LogHttp: '',
                            LogRAM: '',
                            BootNoSD: '',
                            SensorLogFreq: '',
                            sensorFreqOptions: [
                                { label: '10 min', value: 0 },
                                { label: '30 min', value: 1 },
                                { label: '60 min', value: 2 }
                            ]
                          };
                          $scope.pass = {
                            pass: sessionStorage.getItem('pass'),
                            currentPass: '',
                            newPass: '',
                            confirmPass: ''
                          };

                          $scope.loaded = true; //show alternate screen if API fails to refresh
                          $scope.time; //stores moment format of the date on the device
                          $scope.stopTime; //used to stop runningTime counter if needed
                          $scope.loading; //used to show system backdrop
                          $scope.booting; //used to show reboot backdrop
                          $scope.newDate;

                          //functions:
                          $scope.load = load;
                          //$scope.runningTime = runningTime;
                          $scope.saveSettings = saveSettings;
                          $scope.settingsChanged = settingsChanged;
                          $scope.cancelSettingsChanges = cancelSettingsChanges;

                          $scope.savePassword = savePassword;
                          $scope.passwordsChanged = passwordsChanged;
                          $scope.cancelPasswordChanges = cancelPasswordChanges;

                          $scope.saveDate = saveDate;

                          $scope.reboot = reboot;

                          $scope.logOut = logOut;

                          $scope.load();

                          //internals
                          var originalSettings;
                          function settingsChanged() {
                            return !angular.equals($scope.set, originalSettings);
                          }
                          function cancelSettingsChanges() {
                            $scope.set = angular.copy(originalSettings);
                          }

                          var originalPasswords = angular.copy($scope.pass);
                          function passwordsChanged() {
                            return !angular.equals($scope.pass, originalPasswords);
                          }
                          function cancelPasswordChanges() {
                            $scope.pass = angular.copy(originalPasswords);
                          }


                          //#region functions
                          //private:
                          function runningTime() {
                            $scope.time.add(1, 's');
                          }
                          //public:
                          function reboot() {
                            $scope.booting = greenApiService.doReboot().then(
                              function success(data) {
                                $('#collapseBoot').collapse('toggle');
                                NotifierService.info("Device Rebooted!", "Success");
                              },
                              function error(data) {
                                NotifierService.error("could not reboot device.Api failed.", "Error");
                              });
                          }

                          function saveSettings() {
                            //build query:
                            var settings = sessionStorage.getItem('pass') + ($scope.set.LogHttp ? '1' : '0') + ($scope.set.SensorLogFreq.value) + ($scope.set.LogRAM ? '1' : '0') + ($scope.set.BootNoSD ? '1' : '0');
                            $scope.loading = greenApiService.setSettings(settings).then(
                              function success(data) {
                                originalSettings = angular.copy($scope.set);
                                NotifierService.info("Saved.", "Success");
                              },
                              function error(data) {
                                NotifierService.error("could not save settings. Api failed.", "Error");
                              });
                          }

                          function saveDate() {
                            //build query:
                            var time = moment($scope.newDate, 'MMMM Do YYYY, HH:mm:ss (dd)');
                            var arg = time.format("YYMMDDHHmmss") + (1 + parseInt(time.format("e"))); //device count from Sun, moment count from Mon fix
                            $scope.loading = greenApiService.setTime(arg).then(
                              function success(data) {
                                $scope.time = moment(data.data.newdate, "YYYY-MM-DD HH:mm:ss");
                                NotifierService.info("Saved.", "Success");
                              },
                              function error(data) {
                                NotifierService.error("could not save time. Api failed.", "Error");
                              });
                          }

                          function savePassword() {
                            //build query:
                            var settings = $scope.pass.newPass.substring(0, 5) + (originalSettings.LogHttp ? '1' : '0') + (originalSettings.SensorLogFreq.value) + (originalSettings.LogRAM ? '1' : '0') + (originalSettings.BootNoSD ? '1' : '0');
                            $scope.loading = greenApiService.setSettings(settings).then(
                              function success(data) {
                                sessionStorage.setItem('pass', $scope.pass.newPass.substring(0, 5));
                                $scope.pass = angular.copy(originalPasswords);
                                $scope.pass.pass = sessionStorage.getItem('pass');
                                originalPasswords = angular.copy($scope.pass);
                                NotifierService.info("Saved.", "Success");
                              },
                              function error(data) {
                                NotifierService.error("could not save password. Api failed.", "Error");
                              });

                          }

                          function logOut() {
                            sessionStorage.clear();
                            $location.path('/')

                          }

                          // sequential calls both calls at the same time, and one fails (Arduino can not multi task)
                          //$scope.load = $q.all([$scope.promiseSettingPass, $scope.promiseTime]);
                          function load() {
                            $scope.loaded = true;
                            $scope.loading = greenApiService.getSettings().then(
                              function success(data) {
                                var settings = data.data;
                                sessionStorage.setItem('pass', settings.substring(0, 5)); //make user pass is current!
                                $scope.set.LogHttp = settings.substring(5, 6) === '1';
                                $scope.set.SensorLogFreq = $scope.set.sensorFreqOptions[settings.substring(6, 7)];
                                $scope.set.LogRAM = settings.substring(7, 8) === '1';
                                $scope.set.BootNoSD = settings.substring(8, 9) === '1';
                                originalSettings = angular.copy($scope.set);

                                return greenApiService.getTime().then(
                                      function success(data) {
                                        //2015-02-19 12:20:13 3  (1=sunday, 2=monday.... 7-sat) (do not parse e(day of the week) not needed)
                                        $scope.time = moment(data.data.datetime, "YYYY-MM-DD HH:mm:ss");
                                        $scope.stopTime = $interval(runningTime, 1000);
                                      },
                                      function error(e) {
                                        NotifierService.error("Can not load time.", "Error");
                                        $scope.loaded = false;
                                      });
                              },
                              function error(e) {
                                NotifierService.error("Can not load settings.", "Error");
                                $scope.loaded = false;
                              });
                          }
                          //#endregion

                        }]);

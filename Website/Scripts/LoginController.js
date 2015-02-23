angular.module('greenhouse').
      controller('loginController', ['$scope', 'greenApiService', '$location', 'NotifierService',
     function ($scope, greenApiService, $location, NotifierService) {

       $scope.loading; //used to show system backdrop
       $scope.loginpass = '';
       $scope.login = login;

       function login() {
         $scope.loading = greenApiService.isAdmin($scope.loginpass.substring(0, 5)).then(
             function success(data) {
               $('#collapseBoot').collapse('toggle');
               sessionStorage.setItem('pass', $scope.loginpass.substring(0, 5));
               $location.path('/admin');
             },
             function error(data) {
               NotifierService.error("Invalid password", "Error");
             });
       };

     }]);
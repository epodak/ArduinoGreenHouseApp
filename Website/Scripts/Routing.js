angular.module('greenhouse', ['ngRoute', 'ngAnimate', 'cgBusy', 'angularMoment']).
        config(['$routeProvider', function ($routeProvider) {
          $routeProvider.
           when("/", { templateUrl: "Html/home.html", controller: "homeController" }).
           when("/admin", { templateUrl: "Html/admin.html", controller: "adminController" }).
           when("/login", { templateUrl: "Html/login.html", controller: "loginController" }).
           otherwise({ redirectTo: '/' });

        }])
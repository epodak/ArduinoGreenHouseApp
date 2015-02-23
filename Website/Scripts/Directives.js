
angular.module('greenhouse').
  directive('uiDatepicker', function uiDatepickerDirective() {
    'use strict';

    function link(scope, el, attr, ngModel) {
      var input = el.find('input');
      $(el).datetimepicker({
        sideBySide: true,
        showTodayButton: true,
        useCurrent: true,
        showClear: true,
        format: 'MMMM Do YYYY, HH:mm:ss (dd)'
      });
      input.on('blur keyup change', function () {
        ngModel.$setViewValue(input.val());
        //console.log(input.val());
      });
    }

    return {
      restrict: 'A',
      require: 'ngModel',
      scope: { ngModel: '=' },
      link: link
    };
  }).
directive('ngMatch', ['$parse', function ngMatchDirective($parse) {
  'use strict';

  function link(scope, elem, attrs, ctrl) {
    // if ngModel is not defined, we don't need to do anything
    if (!ctrl) return;
    if (!attrs['ngMatch']) return;

    var valueToMatch = $parse(attrs['ngMatch']);

    var validator = function (value) {
      var temp = valueToMatch(scope),
          v = value === temp;
      ctrl.$setValidity('match', v);
      return value;
    }

    ctrl.$parsers.unshift(validator);
    ctrl.$formatters.push(validator);
    attrs.$observe('ngMatch', function () {
      validator(ctrl.$viewValue);
    });

  };
  return {
    restrict: 'A',
    require: '?ngModel',
    link: link
  };
}]);

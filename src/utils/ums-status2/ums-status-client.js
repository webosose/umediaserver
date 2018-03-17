angular.module('ums-status', ['ui.bootstrap'])

.controller('appCtrl', function($scope, $http, $timeout) {
    $scope.data = [];
    $scope.ui = {global:{updating:false, autoupdate:false}};
    $scope.update = function() {
        $scope.ui.global.updating = true;
        $http.get("/update").then(function(response) {
            $scope.data = response.data;
            var ui = {global:$scope.ui.global};
            $scope.data.forEach(function(pipeline){
                var ui_state = $scope.ui[pipeline.id];
                if (!ui_state) {
                    ui_state = {
                        collapsed: true,
                    };
                }
                ui_state.detail = {
                    rm: [
                            ["id", pipeline.id],
                            ["type", pipeline.type],
                            ["managed", pipeline.is_managed],
                            ["foreground", pipeline.is_foreground],
                            ["focus", pipeline.is_focus],
                            ["policy", pipeline.policy_state > 0],
                            ["resources", $scope.resources(pipeline)]
                    ],
                    ums: [],
                    mdc: []
                };
                if (pipeline.is_managed) {
                    ui_state.detail.ums = [
                                ["uri", pipeline.uri],
                                ["pid", pipeline.pid],
                                ["process", pipeline.processState],
                                ["media", pipeline.mediaState],
                                ["app", pipeline.appId]
                    ];
                }
                if (pipeline.mdc) {
                    ui_state.detail.rm[3/*foreground*/][1] = (pipeline.mdc.states.indexOf("Foreground") !== -1);
                    ui_state.detail.mdc = [
                                ["conn", pipeline.mdc.connections],
                                ["states", pipeline.mdc.states]
                    ];
                }

                ui[pipeline.id] = ui_state;
            });
            $scope.ui = ui;
            $timeout(function(){ $scope.ui.global.updating = false; }, 400);
        });
    };
    $scope.autoupdate = function() {
        $scope.ui.global.autoupdate = !$scope.ui.global.autoupdate;
        if ($scope.ui.global.autoupdate) {
            function do_update() {
                if ($scope.ui.global.autoupdate) {
                    $scope.update();
                    setTimeout(do_update, 1000);
                }
            };
            do_update();
        }
    };
    $scope.type = function(pipeline) {
        if (pipeline.processState === 'suspended')
            return 'suspended';
        if ($scope.ui[pipeline.id].detail.rm[3][1])
            return 'foreground';
        if (!pipeline.is_managed)
            return 'unmanaged';
        return 'running';
    };
    $scope.resources = function(pipeline) {
        if (pipeline.resource.length === 0)
            return "none";
        var resources = "";
        for (var res in pipeline.resource) {
            resources += pipeline.resource[res].resource + "[" + pipeline.resource[res].index +"] ";
        }
        return resources;
    };
    $scope.update();
});

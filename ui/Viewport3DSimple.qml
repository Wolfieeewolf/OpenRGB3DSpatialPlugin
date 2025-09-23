import QtQuick 2.15

Rectangle {
    id: root
    color: "#1a1a1a"

    property var controllerModels: []
    property var selectedController: null
    property int gizmoMode: 0

    signal controllerTransformChanged(var controller, var transform)

    // Test content
    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            text: "OpenRGB 3D Spatial Plugin"
            color: "white"
            font.pixelSize: 24
            font.bold: true
        }

        Text {
            text: "Controllers loaded: " + controllerModels.length
            color: "#00ff00"
            font.pixelSize: 18
        }

        Text {
            text: "Gizmo Mode: " + (gizmoMode === 0 ? "Translate" : gizmoMode === 1 ? "Rotate" : "Scale")
            color: "#00aaff"
            font.pixelSize: 16
        }

        Row {
            spacing: 10
            anchors.horizontalCenter: parent.horizontalCenter

            Rectangle {
                width: 100
                height: 40
                color: gizmoMode === 0 ? "#0088ff" : "#333333"
                border.color: "white"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Move"
                    color: "white"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: gizmoMode = 0
                }
            }

            Rectangle {
                width: 100
                height: 40
                color: gizmoMode === 1 ? "#0088ff" : "#333333"
                border.color: "white"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Rotate"
                    color: "white"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: gizmoMode = 1
                }
            }

            Rectangle {
                width: 100
                height: 40
                color: gizmoMode === 2 ? "#0088ff" : "#333333"
                border.color: "white"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Scale"
                    color: "white"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: gizmoMode = 2
                }
            }
        }

        Text {
            text: "Note: Full 3D viewport requires Qt Quick 3D module"
            color: "#888888"
            font.pixelSize: 12
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    function addController(controller) {
        console.log("Adding controller:", controller)
        controllerModels.push(controller)
        controllerModelsChanged()
    }

    function clearControllers() {
        controllerModels = []
        selectedController = null
        controllerModelsChanged()
    }
}
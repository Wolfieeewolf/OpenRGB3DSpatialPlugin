import QtQuick 2.15
import QtQuick.Controls 2.15

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
            text: "3D Viewport (Simple Mode)"
            color: "white"
            font.pixelSize: 24
        }

        Text {
            text: "Controllers: " + controllerModels.length
            color: "#00ff00"
            font.pixelSize: 16
        }

        Row {
            spacing: 10

            Button {
                text: "Move"
                checkable: true
                checked: gizmoMode === 0
                onClicked: gizmoMode = 0
            }

            Button {
                text: "Rotate"
                checkable: true
                checked: gizmoMode === 1
                onClicked: gizmoMode = 1
            }

            Button {
                text: "Scale"
                checkable: true
                checked: gizmoMode === 2
                onClicked: gizmoMode = 2
            }
        }
    }

    function addController(controller) {
        console.log("Adding controller:", controller)
        controllerModels.push(controller)
    }

    function clearControllers() {
        controllerModels = []
        selectedController = null
    }
}
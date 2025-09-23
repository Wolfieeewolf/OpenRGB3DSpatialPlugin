import QtQuick 2.15
import QtQuick3D 6.5

Node {
    id: root

    property var controller: null
    property vector3d controllerPosition: Qt.vector3d(0, 0, 0)
    property vector3d controllerRotation: Qt.vector3d(0, 0, 0)
    property vector3d controllerScale: Qt.vector3d(1, 1, 1)

    position: controllerPosition
    eulerRotation: controllerRotation
    scale: controllerScale

    // LEDs will be created dynamically
    Repeater {
        id: ledRepeater
        model: controller ? controller.ledPositions : []

        Model {
            source: "#Sphere"
            position: Qt.vector3d(
                modelData.localPosition.x,
                modelData.localPosition.y,
                modelData.localPosition.z
            )
            scale: Qt.vector3d(0.15, 0.15, 0.15)

            materials: PrincipledMaterial {
                baseColor: Qt.rgba(
                    modelData.color.r,
                    modelData.color.g,
                    modelData.color.b,
                    1.0
                )
                emissiveColor: Qt.rgba(
                    modelData.color.r * 0.5,
                    modelData.color.g * 0.5,
                    modelData.color.b * 0.5,
                    1.0
                )
                metalness: 0.3
                roughness: 0.4
            }
        }
    }

    // Bounding box when selected
    Model {
        visible: false  // Will be set to true when controller is selected
        source: "#Cube"
        materials: PrincipledMaterial {
            baseColor: Qt.rgba(1, 1, 1, 0.1)
            alphaMode: PrincipledMaterial.Blend
        }
    }
}
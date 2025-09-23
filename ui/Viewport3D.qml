import QtQuick 2.15
import QtQuick3D 6.5
import QtQuick.Controls 2.15

Item {
    id: root

    property var controllerModels: []
    property var selectedController: null
    property int gizmoMode: 0  // 0=translate, 1=rotate, 2=scale

    signal controllerTransformChanged(var controller, var transform)

    View3D {
        id: view3d
        anchors.fill: parent

        environment: SceneEnvironment {
            clearColor: "#1a1a1a"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            id: camera
            position: Qt.vector3d(20, 15, 20)
            eulerRotation.x: -30
            eulerRotation.y: 45

            PropertyAnimation on position.z {
                id: zoomAnimation
                running: false
                duration: 200
            }
        }

        DirectionalLight {
            eulerRotation.x: -45
            eulerRotation.y: 45
            brightness: 1.0
            castsShadow: true
        }

        DirectionalLight {
            eulerRotation.x: 45
            eulerRotation.y: -135
            brightness: 0.5
        }

        // Grid
        Node {
            id: gridNode

            Repeater {
                model: 21

                Model {
                    source: "#Cylinder"
                    position: Qt.vector3d((index - 10), 0, 0)
                    scale: Qt.vector3d(0.02, 0.001, 20)
                    materials: PrincipledMaterial {
                        baseColor: "#404040"
                        metalness: 0
                        roughness: 1
                    }
                }
            }

            Repeater {
                model: 21

                Model {
                    source: "#Cylinder"
                    position: Qt.vector3d(0, 0, (index - 10))
                    scale: Qt.vector3d(20, 0.001, 0.02)
                    materials: PrincipledMaterial {
                        baseColor: "#404040"
                        metalness: 0
                        roughness: 1
                    }
                }
            }

            // Axis indicators
            Model {
                source: "#Cylinder"
                position: Qt.vector3d(2.5, 0, 0)
                scale: Qt.vector3d(0.1, 0.001, 5)
                materials: PrincipledMaterial {
                    baseColor: "#ff0000"
                    metalness: 0
                    roughness: 0.5
                }
            }

            Model {
                source: "#Cylinder"
                position: Qt.vector3d(0, 2.5, 0)
                scale: Qt.vector3d(0.1, 5, 0.1)
                materials: PrincipledMaterial {
                    baseColor: "#00ff00"
                    metalness: 0
                    roughness: 0.5
                }
            }

            Model {
                source: "#Cylinder"
                position: Qt.vector3d(0, 0, 2.5)
                scale: Qt.vector3d(5, 0.001, 0.1)
                materials: PrincipledMaterial {
                    baseColor: "#0000ff"
                    metalness: 0
                    roughness: 0.5
                }
            }
        }

        // Controllers and LEDs will be added dynamically
        Node {
            id: controllersNode
        }

        // Transform Gizmo
        Node {
            id: gizmo
            visible: selectedController !== null
            position: selectedController ? selectedController.position : Qt.vector3d(0, 0, 0)

            // Translate Gizmo
            Node {
                visible: gizmoMode === 0

                // X Axis
                Model {
                    source: "#Cylinder"
                    position: Qt.vector3d(1.5, 0, 0)
                    eulerRotation.z: -90
                    scale: Qt.vector3d(0.1, 3, 0.1)
                    materials: PrincipledMaterial {
                        baseColor: "#ff0000"
                        emissiveColor: "#ff0000"
                        metalness: 0
                        roughness: 0.3
                    }

                    Model {
                        source: "#Cone"
                        position: Qt.vector3d(0, 1.8, 0)
                        scale: Qt.vector3d(0.3, 0.5, 0.3)
                        materials: PrincipledMaterial {
                            baseColor: "#ff0000"
                            emissiveColor: "#ff0000"
                        }
                    }
                }

                // Y Axis
                Model {
                    source: "#Cylinder"
                    position: Qt.vector3d(0, 1.5, 0)
                    scale: Qt.vector3d(0.1, 3, 0.1)
                    materials: PrincipledMaterial {
                        baseColor: "#00ff00"
                        emissiveColor: "#00ff00"
                        metalness: 0
                        roughness: 0.3
                    }

                    Model {
                        source: "#Cone"
                        position: Qt.vector3d(0, 1.8, 0)
                        scale: Qt.vector3d(0.3, 0.5, 0.3)
                        materials: PrincipledMaterial {
                            baseColor: "#00ff00"
                            emissiveColor: "#00ff00"
                        }
                    }
                }

                // Z Axis
                Model {
                    source: "#Cylinder"
                    position: Qt.vector3d(0, 0, 1.5)
                    eulerRotation.x: 90
                    scale: Qt.vector3d(0.1, 3, 0.1)
                    materials: PrincipledMaterial {
                        baseColor: "#0000ff"
                        emissiveColor: "#0000ff"
                        metalness: 0
                        roughness: 0.3
                    }

                    Model {
                        source: "#Cone"
                        position: Qt.vector3d(0, 1.8, 0)
                        scale: Qt.vector3d(0.3, 0.5, 0.3)
                        materials: PrincipledMaterial {
                            baseColor: "#0000ff"
                            emissiveColor: "#0000ff"
                        }
                    }
                }
            }

            // Rotate Gizmo
            Node {
                visible: gizmoMode === 1

                Model {
                    source: "#Sphere"
                    scale: Qt.vector3d(3, 3, 3)
                    materials: PrincipledMaterial {
                        baseColor: Qt.rgba(1, 1, 0, 0.3)
                        alphaMode: PrincipledMaterial.Blend
                    }
                }
            }

            // Scale Gizmo
            Node {
                visible: gizmoMode === 2

                Model {
                    source: "#Cube"
                    position: Qt.vector3d(1.5, 0, 0)
                    scale: Qt.vector3d(0.3, 0.3, 0.3)
                    materials: PrincipledMaterial {
                        baseColor: "#ff0000"
                    }
                }

                Model {
                    source: "#Cube"
                    position: Qt.vector3d(0, 1.5, 0)
                    scale: Qt.vector3d(0.3, 0.3, 0.3)
                    materials: PrincipledMaterial {
                        baseColor: "#00ff00"
                    }
                }

                Model {
                    source: "#Cube"
                    position: Qt.vector3d(0, 0, 1.5)
                    scale: Qt.vector3d(0.3, 0.3, 0.3)
                    materials: PrincipledMaterial {
                        baseColor: "#0000ff"
                    }
                }
            }
        }
    }

    // Camera Controls
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.MiddleButton | Qt.RightButton

        property point lastPos

        onPressed: {
            lastPos = Qt.point(mouse.x, mouse.y)
        }

        onPositionChanged: {
            if (pressed) {
                var dx = mouse.x - lastPos.x
                var dy = mouse.y - lastPos.y

                if (mouse.buttons & Qt.MiddleButton) {
                    // Orbit camera
                    camera.eulerRotation.y += dx * 0.5
                    camera.eulerRotation.x -= dy * 0.5

                    // Clamp pitch
                    if (camera.eulerRotation.x > 89) camera.eulerRotation.x = 89
                    if (camera.eulerRotation.x < -89) camera.eulerRotation.x = -89
                }

                lastPos = Qt.point(mouse.x, mouse.y)
            }
        }

        onWheel: {
            var delta = wheel.angleDelta.y / 120
            var currentDist = camera.position.length()
            var newDist = currentDist - delta * 2

            if (newDist < 5) newDist = 5
            if (newDist > 100) newDist = 100

            var dir = camera.position.normalized()
            camera.position = dir.times(newDist)
        }
    }

    // Toolbar
    Row {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        spacing: 5

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

    function addController(controller) {
        var component = Qt.createComponent("ControllerModel.qml")
        if (component.status === Component.Ready) {
            var model = component.createObject(controllersNode, {
                "controller": controller
            })
            controllerModels.push(model)
        }
    }

    function clearControllers() {
        for (var i = 0; i < controllerModels.length; i++) {
            controllerModels[i].destroy()
        }
        controllerModels = []
        selectedController = null
    }
}
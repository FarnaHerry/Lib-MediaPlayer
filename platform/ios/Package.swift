// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "Lib-MediaPlayer",
    platforms: [
        .iOS(.v15),
    ],
    products: [
        .library(
            name: "MediaPlayer",
            targets: ["MediaPlayer"]
        ),
    ],
    targets: [
        .target(name: "MediaPlayer"),
    ]
)

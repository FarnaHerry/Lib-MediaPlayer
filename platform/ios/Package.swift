// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "Lib-Media",
    platforms: [
        .iOS(.v15),
    ],
    products: [
        .library(
            name: "Media",
            targets: ["Media"]
        ),
    ],
    targets: [
        .target(
            name: "Media",
            linkerSettings: [
                .linkedFramework("AVFoundation"),
                .linkedFramework("QuartzCore"),
                .linkedFramework("CoreMedia"),
                .linkedFramework("Photos"),
                .linkedFramework("PhotosUI"),
                .linkedFramework("UniformTypeIdentifiers"),
            ]
        ),
    ]
)

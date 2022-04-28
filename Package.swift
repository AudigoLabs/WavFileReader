// swift-tools-version: 5.6
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "WavFileReader",
    platforms: [.iOS(.v11)],
    products: [
        .library(name: "WavFileReader", targets: ["WavFileReader"]),
    ],
    dependencies: [],
    targets: [
        .target(name: "WavFileReader", dependencies: ["CWavFileReader"]),
        .target(name: "CWavFileReader", cxxSettings: [.headerSearchPath(".")]),
        .testTarget(name: "WavFileReaderTests", dependencies: ["WavFileReader"], resources: [.copy("TestResources")]),
    ],
    cxxLanguageStandard: .cxx14
)

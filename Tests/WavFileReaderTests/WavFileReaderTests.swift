import WavFileReader
import XCTest
import AVFAudio

final class WavFileReaderTests: XCTestCase {

    func testProperties() throws {
        let url = Bundle.module.url(forResource: "drum", withExtension: "wav", subdirectory: "TestResources")!
        let reader = try WavFileReader(url: url)
        XCTAssertEqual(reader.format.channelCount, 2)
        XCTAssertEqual(reader.format.sampleRate, 44100)
        XCTAssertEqual(reader.duration, 19.2)
    }

    func testRead() throws {
        // Open the file using our reader
        let url = Bundle.module.url(forResource: "drum", withExtension: "wav", subdirectory: "TestResources")!
        let reader = try WavFileReader(url: url)

        // Compare our reader with AVAudioFile
        let audioFile = try AVAudioFile(forReading: url)

        // Compare the number of frames
        let expectedNumFrames = AVAudioFrameCount(audioFile.length)
        XCTAssertEqual(Double(expectedNumFrames) / reader.format.sampleRate, 19.2)

        // Read the expected file
        let expectedBuffer = AVAudioPCMBuffer(pcmFormat: reader.format, frameCapacity: expectedNumFrames)!
        try audioFile.read(into: expectedBuffer)

        // Read the entire file using our reader
        let buffer = reader.readFrames(frameCapacity: expectedNumFrames)
        XCTAssertEqual(buffer.frameLength, expectedNumFrames)

        // Compare each channel
        for i in 0..<Int(reader.format.channelCount) {
            XCTAssertEqual(memcmp(buffer.floatChannelData![i], expectedBuffer.floatChannelData![i], Int(expectedNumFrames) * MemoryLayout<Float>.stride), 0)
        }
    }

    func testSeekAndRead() throws {
        // Open the file using our reader
        let url = Bundle.module.url(forResource: "drum", withExtension: "wav", subdirectory: "TestResources")!
        let reader = try WavFileReader(url: url)

        // Compare our reader with AVAudioFile
        let audioFile = try AVAudioFile(forReading: url)
        audioFile.framePosition = Int64(0.6 * reader.format.sampleRate + 0.5)

        // Read the expected buffer
        let expectedBuffer = AVAudioPCMBuffer(pcmFormat: reader.format, frameCapacity: 100)!
        try audioFile.read(into: expectedBuffer)

        // Read the buffer using our reader
        try reader.seek(position: 0.6)
        let buffer = reader.readFrames(frameCapacity: 100)
        XCTAssertEqual(buffer.frameLength, 100)
    }

}

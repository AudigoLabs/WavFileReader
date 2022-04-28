import CWavFileReader
import Foundation
import AVFoundation

public class WavFileReader {

    public let format: AVAudioFormat

    private let handle: wav_file_handle_t

    public init(url: URL) throws {
        guard let path = url.path.cString(using: .utf8) else {
            throw WavFileReaderError.invalidPath("Failed to convert path to C string")
        }
        var handle: wav_file_handle_t?
        let result = wav_file_open(path, &handle)
        guard result.isSuccess else {
            throw result.toWavFileReaderError()
        }
        guard let handle = handle else {
            fatalError("Did not get handle back from successful open")
        }
        let numChannels = AVAudioChannelCount(wav_file_get_num_channels(handle))
        let sampleRate = Double(wav_file_get_sample_rate(handle))
        guard let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: numChannels) else {
            throw WavFileReaderError.invalidFormat
        }
        self.handle = handle
        self.format = format
    }

    deinit {
        wav_file_close(handle)
    }

    public var duration: TimeInterval {
        wav_file_get_duration(handle)
    }

    public func readFrames(frameCapacity: UInt32) -> AVAudioPCMBuffer {
        guard let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: frameCapacity),
              let floatChannelData = buffer.floatChannelData else {
            fatalError("Failed to create buffer")
        }
        buffer.frameLength = wav_file_read(handle, floatChannelData, buffer.frameCapacity)
        return buffer
    }

    public func seek(position: TimeInterval) throws {
        let result = wav_file_set_seek(handle, position)
        guard result.isSuccess else {
            throw result.toWavFileReaderError()
        }
    }

}

import CWavFileReader
import Foundation

public enum WavFileReaderError : Error {

    case invalidPath(String)
    case invalidFormat

    case openFailed
    case fileError
    case unsupported
    case invalidParam
    case unknownCError(UInt32)

}

extension wav_file_result_t {

    var isSuccess: Bool { self == WAV_FILE_RESULT_SUCCESS }

    func toWavFileReaderError() -> WavFileReaderError {
        switch self {
        case WAV_FILE_RESULT_OPEN_FAILED:
            return .openFailed
        case WAV_FILE_RESULT_FILE_ERROR:
            return .fileError
        case WAV_FILE_RESULT_UNSUPPORTED:
            return .unsupported
        case WAV_FILE_RESULT_INVALID_PARAM:
            return .invalidParam
        default:
            return .unknownCError(rawValue)
        }
    }

}

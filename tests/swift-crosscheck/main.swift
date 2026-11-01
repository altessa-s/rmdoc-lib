// Copyright 2026 ALTESSA SOLUTIONS INC.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.

import Foundation
func code(_ error: Error) -> Int {
    if let e = error as? RMDocError {
        switch e {
        case .invalidName: return 4
        case .duplicateName: return 5
        case .emptyArchive: return 6
        case .fileTooLarge: return 7
        case .archiveTooLarge: return 8
        case .notAnArchive: return 9
        case .malformedArchive: return 10
        case .unsupportedCompression: return 11
        case .checksumMismatch: return 12
        case .archiveFinished: return 13
        }
    }
    if let e = error as? RMDocReadError {
        switch e {
        case .tooManyEntries: return 14
        case .entryTooLarge: return 15
        case .totalSizeExceeded: return 16
        case .compressionRatioExceeded: return 17
        case .compressedEntryTooLarge: return 18
        case .compressedTotalSizeExceeded: return 19
        case .directoryTooLarge: return 20
        case .nameTooLong: return 21
        case .zip64Unsupported: return 22
        case .encryptedEntry: return 23
        case .spannedArchive: return 24
        }
    }
    return 3
}
while let line = readLine() {
    do {
        let request = try JSONSerialization.jsonObject(with: Data(line.utf8)) as! [String: Any]
        let data = Data(base64Encoded: request["data"] as! String)!
        let list = request["list"] as? Bool ?? false
        let numbers = request["limits"] as? [Int]
        let limits: RMDocReadLimits
        if let v = numbers {
            limits = RMDocReadLimits(maximumEntryCount: v[0], maximumEntrySize: v[1],
                maximumTotalSize: v[2], maximumCompressionRatio: v[3], compressionRatioThreshold: v[4],
                maximumCompressedEntrySize: v[5], maximumCompressedTotalSize: v[6],
                maximumDirectorySize: v[7], maximumNameLength: v[8])
        } else { limits = .default }
        var response: [String: Any] = ["status": 0]
        if list {
            let entries = try RMDocArchive.entries(in: data, limits: limits)
            response["names"] = entries.map(\.name)
        } else {
            var files: [RMDocFile] = []
            let chosen = request["select"] as? [String]
            try RMDocArchive.forEachFile(unpacking: data, limits: limits, where: { chosen == nil || chosen!.contains($0.name) }) { files.append($0) }
            response["names"] = files.map(\.name)
            response["payloads"] = files.map { $0.data.base64EncodedString() }
            if !files.isEmpty { response["packed"] = try RMDocArchive(files: files).packed().base64EncodedString() }
        }
        let encoded = try JSONSerialization.data(withJSONObject: response, options: [.sortedKeys])
        print(String(decoding: encoded, as: UTF8.self))
    } catch { print("{\"status\":\(code(error))}") }
}

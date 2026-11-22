// WritePreset - writes a Tonbox preset (.tonbox) from a list of parameter values.
//
//   WritePreset <params.txt> <preset name> <output.tonbox>
//
// params.txt holds "id value" lines (Tonbox's parameter ids, in the units
// Tonbox stores) and "@id text" lines for the names (kitName, name1..name8),
// one per line.
//
// The preset is made by the installed Tonbox itself, so its state is whatever
// the plugin writes: the unit's own state is taken, the values are written
// into its parameter tree (the XML inside "jucePluginState"), the unit loads
// that, and its state is read back out. Every value is then checked against
// what the unit actually holds, and the tree is saved the way the plugin's
// PRESETS menu saves it (Source/Engine/PresetFile.h).

import AVFoundation
import AudioToolbox
import Foundation

func fail(_ message: String) -> Never {
    FileHandle.standardError.write((message + "\n").data(using: .utf8)!)
    exit(1)
}

func fourCC(_ s: String) -> FourCharCode {
    s.utf8.reduce(0) { ($0 << 8) | FourCharCode($1) }
}

let args = CommandLine.arguments
guard args.count == 4 else { fail("usage: WritePreset <params.txt> <preset name> <output.tonbox>") }

// The values.
var values: [(id: String, value: String)] = []
var names: [(id: String, text: String)] = []
for line in try! String(contentsOfFile: args[1], encoding: .utf8).split(separator: "\n", omittingEmptySubsequences: true) {
    if line.hasPrefix("@") {
        let body = line.dropFirst()
        let id = body.prefix { $0 != " " }
        names.append((String(id), String(body.dropFirst(id.count).drop { $0 == " " })))
        continue
    }
    let fields = line.split(separator: " ")
    guard fields.count == 2 else { continue }
    values.append((String(fields[0]), String(fields[1])))
}

func escape(_ text: String) -> String {
    text.replacingOccurrences(of: "&", with: "&amp;").replacingOccurrences(of: "<", with: "&lt;")
        .replacingOccurrences(of: ">", with: "&gt;").replacingOccurrences(of: "\"", with: "&quot;")
        .replacingOccurrences(of: "'", with: "&apos;")
}

func unescape(_ text: String) -> String {
    text.replacingOccurrences(of: "&quot;", with: "\"").replacingOccurrences(of: "&apos;", with: "'")
        .replacingOccurrences(of: "&lt;", with: "<").replacingOccurrences(of: "&gt;", with: ">")
        .replacingOccurrences(of: "&amp;", with: "&")
}

/** The names are attributes of the root element, <Tonbox kitName="..." name1="..." ...>. */
func rootAttribute(_ xml: String, _ id: String) -> String? {
    guard let rootStart = xml.range(of: "<Tonbox"), let rootEnd = xml[rootStart.upperBound...].firstIndex(of: ">") else { return nil }
    let root = xml[rootStart.upperBound..<rootEnd]
    guard let range = root.range(of: " \(id)=\"") else { return nil }
    let rest = root[range.upperBound...]
    guard let end = rest.firstIndex(of: "\"") else { return nil }
    return unescape(String(rest[..<end]))
}

func setRootAttribute(_ xml: inout String, _ id: String, _ text: String) {
    guard let rootStart = xml.range(of: "<Tonbox"), let rootEnd = xml[rootStart.upperBound...].firstIndex(of: ">") else {
        fail("no <Tonbox> element in the state")
    }
    if let range = xml[rootStart.upperBound..<rootEnd].range(of: " \(id)=\""),
       let end = xml[range.upperBound...].firstIndex(of: "\"") {
        xml.replaceSubrange(range.upperBound..<end, with: escape(text))
    } else {
        xml.insert(contentsOf: " \(id)=\"\(escape(text))\"", at: rootStart.upperBound)
    }
}

// The unit.
let desc = AudioComponentDescription(componentType: kAudioUnitType_MusicDevice, componentSubType: fourCC("Tnbx"),
                                     componentManufacturer: fourCC("Tzor"), componentFlags: 0, componentFlagsMask: 0)
var loaded: AVAudioUnit?
let done = DispatchSemaphore(value: 0)
AVAudioUnit.instantiate(with: desc, options: .loadInProcess) { unit, _ in loaded = unit; done.signal() }
done.wait()
guard let unit = loaded else { fail("Tonbox (aumu Tnbx Tzor) is not installed") }

// JUCE's state: a magic number, the length, the XML, a terminating zero.
let magic: UInt32 = 0x21324356

func unpack(_ data: Data) -> String {
    let bytes = [UInt8](data)
    guard bytes.count > 8 else { fail("state too short") }
    let head = bytes[0..<4].withUnsafeBytes { $0.loadUnaligned(as: UInt32.self) }
    let size = Int(bytes[4..<8].withUnsafeBytes { $0.loadUnaligned(as: UInt32.self) })
    guard head == magic, 8 + size <= bytes.count else { fail("not a JUCE XML state") }
    return String(decoding: bytes[8..<(8 + size)], as: UTF8.self)
}

func pack(_ xml: String) -> Data {
    var data = Data()
    var head = magic.littleEndian, size = UInt32(xml.utf8.count).littleEndian
    withUnsafeBytes(of: &head) { data.append(contentsOf: $0) }
    withUnsafeBytes(of: &size) { data.append(contentsOf: $0) }
    data.append(contentsOf: Array(xml.utf8))
    data.append(0)
    return data
}

func valueIn(_ xml: String, _ id: String) -> Double? {
    guard let range = xml.range(of: "id=\"\(id)\" value=\"") else { return nil }
    let rest = xml[range.upperBound...]
    guard let end = rest.firstIndex(of: "\"") else { return nil }
    return Double(rest[..<end])
}

guard var state = unit.auAudioUnit.fullState, let pluginState = state["jucePluginState"] as? Data else {
    fail("Tonbox returned no state")
}

var xml = unpack(pluginState)

for (id, value) in values {
    guard let range = xml.range(of: "id=\"\(id)\" value=\"") else { fail("Tonbox has no parameter \(id)") }
    let rest = xml[range.upperBound...]
    guard let end = rest.firstIndex(of: "\"") else { fail("malformed state at \(id)") }
    xml.replaceSubrange(range.upperBound..<end, with: value)
}

for (id, text) in names {
    setRootAttribute(&xml, id, text)
}

state["jucePluginState"] = pack(xml)
unit.auAudioUnit.fullState = state

// What the unit made of it.
guard let loadedState = unit.auAudioUnit.fullState, let back = loadedState["jucePluginState"] as? Data else {
    fail("Tonbox returned no state after loading")
}

let loadedXml = unpack(back)
var mismatches = 0
for (id, value) in values {
    let want = Double(value)!, got = valueIn(loadedXml, id) ?? .nan
    if !(abs(got - want) <= 1.0e-4 * max(1.0, abs(want))) {
        mismatches += 1
        print("  \(id): wanted \(want), unit holds \(got)")
    }
}
for (id, text) in names where rootAttribute(loadedXml, id) != text {
    mismatches += 1
    print("  \(id): wanted \"\(text)\", unit holds \"\(rootAttribute(loadedXml, id) ?? "-")\"")
}
guard mismatches == 0 else { fail("\(mismatches) value(s) did not survive loading") }

// The tree without JUCE's XML declaration and terminating zero, inside the
// root PresetFile.h writes.
var tree = loadedXml.replacingOccurrences(of: "\0", with: "")
if let declaration = tree.range(of: "?>"), tree.hasPrefix("<?xml") { tree = String(tree[declaration.upperBound...]) }
tree = tree.trimmingCharacters(in: .whitespacesAndNewlines)
let preset = """
<?xml version="1.0" encoding="UTF-8"?>

<TZorrPreset plugin="Tnbx" product="Tonbox" formatVersion="1" name="\(escape(args[2]))">
\(tree)
</TZorrPreset>

"""
// Indented, one parameter per line, as the plugin's own saves are.
guard let document = try? XMLDocument(xmlString: preset, options: []) else { fail("the preset is not well-formed XML") }
let pretty = document.xmlString(options: [.nodePrettyPrint, .nodeCompactEmptyElement]) + "\n"
try! pretty.write(to: URL(fileURLWithPath: args[3]), atomically: true, encoding: .utf8)
print("\(values.count) values and \(names.count) names loaded and checked; wrote \(args[3])")

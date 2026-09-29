require 'json'
require 'fileutils'
require_relative 'vendor_firmware'

abort "Usage: ruby #{File.basename($PROGRAM_NAME)} VENDOR_OBJECT OUTPUT_DIRECTORY" unless ARGV.size == 2
firmware = VendorFirmware.new(ARGV[0])
image = firmware.image
manifest = {
  model: 'original VGA2USB', vendor_id: '5555', product_id: '1110',
  vendor_object_sha256: VendorFirmware::SOURCE_SHA256,
  firmware_bcd_device: format('%04x', firmware.version),
  decoded_bytes: firmware.segments.sum { |s| s[:length] },
  image_format: 'V2UF; LE16 VID, PID, version, segment count; repeated LE32 address, length, data',
  image_sha256: Digest::SHA256.hexdigest(image),
  segments: firmware.segments.map { |s| s.reject { |key, _value| key == :data } }
}
FileUtils.mkdir_p(ARGV[1])
File.binwrite(File.join(ARGV[1], 'original-vga2usb.v2uf'), image)
File.write(File.join(ARGV[1], 'manifest.json'), JSON.pretty_generate(manifest) + "\n")
puts "original VGA2USB: #{firmware.segments.size} segments, #{manifest[:decoded_bytes]} firmware bytes, version #{manifest[:firmware_bcd_device]}"
puts "image SHA256 #{manifest[:image_sha256]}"

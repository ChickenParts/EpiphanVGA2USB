require 'fileutils'
require_relative 'vendor_firmware'
abort 'Usage: render_firmware.rb VENDOR_OBJECT OUTPUT_HEADER' unless ARGV.size == 2
firmware = VendorFirmware.new(ARGV[0])
image_hash = Digest::SHA256.hexdigest(firmware.image)
abort 'firmware image hash mismatch' unless image_hash == 'eb61be5636ab3cc73f1042f432dd9cbc7e47f6cbc561ea3a9abd1106abcebdec'
FileUtils.mkdir_p(File.dirname(ARGV[1]))
File.open(ARGV[1], 'w') do |file|
  file.puts '/* Epiphan-owned firmware extracted from the pinned official object. */'
  file.puts '#include "bootstrap.h"'
  file.puts "static const char firmware_hash[] = \"#{image_hash}\";"
  file.puts "static const uint16_t firmware_version = #{firmware.version};"
  firmware.segments.each do |segment|
    file.puts "static const uint8_t firmware_data_#{segment[:index]}[] = {"
    segment[:data].bytes.each_slice(16) { |row| file.puts row.map { |byte| format('0x%02x', byte) }.join(',') + ',' }
    file.puts '};'
  end
  file.puts 'static const struct v2u_segment firmware_segments[] = {'
  firmware.segments.each do |segment|
    file.puts "{#{segment[:address]},#{segment[:length]},firmware_data_#{segment[:index]}},"
  end
  file.puts '};'
end

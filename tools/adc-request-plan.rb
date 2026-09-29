#!/usr/bin/env ruby
# Offline evidence encoder/decoder. This program has no USB or network access.
require 'json'

def request(type, command, value, index, length)
  { bmRequestType: type, bRequest: command, wValue: value,
    wIndex: index, wLength: length,
    setup_hex: [type, command, value, index, length].pack('CCvvv').unpack1('H*') }
end

case ARGV.shift || 'mode-plan'
when 'mode-plan'
  puts JSON.pretty_generate([request(0xc0, 0xb1, 0, 0, 10)])
when 'i2c-read-plan'
  # A caller must explicitly select the register and length; no register probe
  # is recommended by default. These fields reproduce the proven USB bridge.
  register = Integer(ARGV.fetch(0), 0)
  length = Integer(ARGV.fetch(1), 0)
  abort 'register must be 0..255 and length 1..64' unless (0..255).cover?(register) && (1..64).cover?(length)
  puts JSON.pretty_generate([request(0xc0, 0xb7, 0x4c, (length << 8) | register, length)])
when 'decode-mode'
  hex = ARGV.fetch(0).delete(' :')
  abort 'expected exactly 10 bytes of hex' unless hex.match?(/\A[0-9a-fA-F]{20}\z/)
  bytes = [hex].pack('H*').bytes
  words = [hex[0, 16]].pack('H*').unpack('n4')
  result = { period_counter: words[0], line_count: words[1],
             other_counter_2: words[2], other_counter_3: words[3],
             status_byte: bytes[8], unclassified_byte: bytes[9],
             status_low_two_bits: bytes[8] & 3, signal_present: words[0] != 0 }
  unless words[0].zero?
    result[:vendor_refresh_millihz] = 2_000_000_000 / words[0]
    result[:vendor_match_refresh_tenths_hz] = 20_000_000 / words[0]
  end
  puts JSON.pretty_generate(result)
else
  abort 'use mode-plan, i2c-read-plan REGISTER LENGTH, or decode-mode HEX'
end

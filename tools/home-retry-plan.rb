#!/usr/bin/env ruby
# Offline payload planner only: never opens a USB device or sends a request.
hex = ARGV.fetch(0).delete(' \n\t')
abort 'expected a 31-byte hexadecimal B4 payload' unless hex.match?(/\A[0-9a-fA-F]{62}\z/)
bytes = [hex].pack('H*').bytes
height = bytes[16] * 256 + bytes[17]
v = bytes[25]
abort 'inconsistent vertical phase fields' unless bytes[18] * 256 + bytes[19] == v
candidate = ((v + 1)..254).find { |n| height % n == 0 }
abort 'vendor phase search exhausted: no divisor below 255' unless candidate
bytes[18], bytes[19] = candidate >> 8, candidate & 255
lines = height / candidate
bytes[20], bytes[21] = lines >> 8, lines & 255
bytes[25] = candidate
puts "V=#{v} -> #{candidate}; lines per phase=#{lines}; payload bytes=#{bytes.length}"
puts bytes.map { |b| '%02x' % b }.join(' ')

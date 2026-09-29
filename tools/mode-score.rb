#!/usr/bin/env ruby
# Offline original AD9884A call shape: unknown interlace, no polarity constraints,
# no sync-width constraint, no optional resolution preference.
require 'json'
rodata_path, period_text, lines_text, preference_text = ARGV
prefer_wide = preference_text == "1"
period = Integer(period_text)
lines = Integer(lines_text)
abort 'period must be positive' unless period.positive?
refresh_tenths = 20_000_000 / period
data = File.binread(rodata_path)
scored = 137.times.filter_map do |index|
  address = 0x1a3fc0 + 24 * index
  refresh, width, hf, hs, hb, height, vf, vs, vb, flags = data.byteslice(address, 24).unpack('Vv8V')
  next unless flags & 3 == 3
  interlaced = flags & 0x20 != 0
  next if (interlaced ? (height + 6) / 2 : height + 6) > lines
  blanking = vf + vs + vb
  total = height + blanking + (interlaced ? blanking + 1 : 0)
  total /= 2 if interlaced
  score = ((refresh + 49) / 100 - refresh_tenths)**2 + (total - lines)**2
  score += 300 - 3 * (width * 100 / height) if prefer_wide
  { index: index, address: "0x#{address.to_s(16)}", refresh_millihz: refresh,
    width: width, height: height, horizontal_parts: [width, hf, hs, hb],
    vertical_parts: [height, vf, vs, vb], flags: flags, score: score }
end
puts JSON.pretty_generate({ period: period, lines: lines, prefer_wide: prefer_wide, match_refresh_tenths_hz: refresh_tenths,
                            candidates: scored.select { |row| row[:score] < 3000 }.sort_by { |row| [row[:score], row[:index]] } })

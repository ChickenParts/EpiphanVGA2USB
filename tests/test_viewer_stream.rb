require 'json'
require 'open3'
require 'fileutils'
require 'tmpdir'
require 'zlib'

def check(condition, message)
  abort message unless condition
end

Dir.mktmpdir('vga2usb-viewer') do |root|
  FileUtils.mkdir_p(["#{root}/tools", "#{root}/build", "#{root}/bin"])
  FileUtils.cp(File.expand_path('../tools/view-live.sh', __dir__), "#{root}/tools/view-live.sh")
  File.write("#{root}/bin/sudo", "#!/bin/sh\nshift\nexec \"$@\"\n")
  File.write("#{root}/build/vga2usb-native", <<~'RUBY')
    #!/usr/bin/env ruby
    STDOUT.binmode
    mode = ARGV.each_cons(2).any? { |a,b| a == '--mode' && b == '640x480' }
    width, height = mode ? [640,480] : [720,400]
    red = "\xff\x00\x00".b * (width*height)
    black = "\x00\x00\x00".b * (width*height)
    green = "\x00\xff\x00".b * (width*height)
    600.times do |i|
      STDOUT.write("P6\n#{width} #{height}\n255\n")
      STDOUT.write(i == 200 ? black : i > 200 ? green : red)
    end
  RUBY
  File.write("#{root}/bin/ffplay", <<~'RUBY')
    #!/usr/bin/env ruby
    require 'json'
    File.write(ENV.fetch('VIEWER_ARGS'), JSON.generate(ARGV))
    inputs = %w[-f -vcodec -framerate -use_wallclock_as_timestamps -fflags -probesize -analyzeduration -frame_size]
    command = ['ffmpeg','-hide_banner','-loglevel','warning']
    ARGV.each_with_index { |arg,i| command.concat([arg,ARGV.fetch(i+1)]) if inputs.include?(arg) }
    command.concat(['-i','-','-fps_mode','passthrough','-f','framecrc','-'])
    exec(*command)
  RUBY
  %W[#{root}/tools/view-live.sh #{root}/bin/sudo #{root}/bin/ffplay #{root}/build/vga2usb-native].each { |p| File.chmod(0755,p) }
  [[], ['--mode','640x480']].each do |args|
    env = {'PATH'=>"#{root}/bin:#{ENV.fetch('PATH')}", 'VIEWER_ARGS'=>"#{root}/args.json"}
    frames, diagnostics, status = Open3.capture3(env, "#{root}/tools/view-live.sh", *args)
    check(status.success?, "Viewer input failed: #{diagnostics}")
    check(diagnostics.empty?, "Valid PPM frames produced parser errors: #{diagnostics}")
    data = frames.lines.reject { |line| line.start_with?('#') }.map { |line| line.split(',').map(&:strip) }
    # nobuffer deliberately discards the first packet used for stream discovery.
    check(data.size == 599, "Lost complete frames: #{data.size}/599")
    pixels = args.empty? ? 720*400 : 640*480
    check(data.all? { |row| row[4].to_i == pixels*3 }, 'Decoded frame geometry changed')
    checksums = data.map(&:last)
    check(checksums.count('0x00000000') == 1, 'Signal-loss clear frame was lost or duplicated')
    red_crc = format('0x%08x', Zlib.adler32("\xff\x00\x00".b * pixels, 0))
    green_crc = format('0x%08x', Zlib.adler32("\x00\xff\x00".b * pixels, 0))
    check(checksums.count(red_crc) == 199, 'Pre-loss red frames were corrupted or lost')
    check(checksums.count(green_crc) == 399, 'Returned green frames were corrupted or lost')
  end
end
puts 'Viewer input: complete frames, clear frame and returned pixels decode without parser errors in both modes'

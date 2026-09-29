# Frozen source reference: Epiphan vga2usb 3.33.0.17, Ubuntu 5.4.0-91 package.
require 'digest'
require 'zlib'

class VendorFirmware
  SOURCE_SHA256 = '536b9be25b310d57a77d2888fe83cd2e499417dbb963129edc90357cf9736dbb'
  attr_reader :segments, :version

  def self.decode(method, input, expected)
    raise ArgumentError, 'invalid segment size' unless expected.between?(1, 0x4000)
    output = ''.b
    case method
    when 0
      output = input.b
    when 1
      state = 0
      last = nil
      input.each_byte do |byte|
        if state == 2
          output << last.chr.b * byte
          state = 0
        else
          output << byte
          state = state == 1 && last == byte ? 2 : 1
          last = byte
        end
        raise ArgumentError, 'RLE output exceeds segment' if output.bytesize > expected
      end
      raise ArgumentError, 'missing RLE repeat count' if state == 2
    when 2
      inflater = Zlib::Inflate.new
      begin
        inflater.inflate(input) do |chunk|
          raise ArgumentError, 'zlib output exceeds segment' if output.bytesize + chunk.bytesize > expected
          output << chunk
        end
        pending = inflater.flush_next_out
        raise ArgumentError, 'zlib output exceeds segment' if output.bytesize + pending.bytesize > expected
        output << pending
      rescue Zlib::Error => error
        raise ArgumentError, "invalid zlib segment: #{error.message}"
      ensure
        inflater.close
      end
      # Vendor streams omit trailers. Exact decoded length is the completion rule.
    else
      raise ArgumentError, "unknown compression method #{method}"
    end
    raise ArgumentError, "segment decoded #{output.bytesize}/#{expected} bytes" unless output.bytesize == expected
    output
  end

  def initialize(path)
    @data = File.binread(path)
    raise ArgumentError, 'vendor object SHA256 mismatch' unless Digest::SHA256.hexdigest(@data) == SOURCE_SHA256
    raise ArgumentError, 'expected little-endian ELF64 relocatable x86-64' unless
      slice(0, 6) == "\x7fELF\x02\x01".b && slice(16, 4).unpack('v2') == [1, 62]
    shoff = slice(40, 8).unpack1('Q<')
    entsize, count, string_index = slice(58, 6).unpack('v3')
    raise ArgumentError, 'unexpected section layout' unless entsize == 64
    @sections = count.times.map { |i| slice(shoff + i * entsize, 64).unpack('V2Q<4V2Q<2') }
    names = section_data(string_index)
    @section_names = @sections.map { |s| names.byteslice(s[0]..).split("\0", 2).first }
    @ro_index = @section_names.index('.rodata') or raise ArgumentError, 'missing rodata'
    @ro = section_data(@ro_index)
    sym_index = @section_names.index('.symtab') or raise ArgumentError, 'missing symtab'
    strings = section_data(@sections[sym_index][6])
    @symbols = section_data(sym_index).bytes.each_slice(24).map do |bytes|
      name, _info, _other, sec, value, size = bytes.pack('C*').unpack('VCCvQ<Q<')
      {name: strings.byteslice(name..).split("\0", 2).first, section: sec, value: value, size: size}
    end
    rel_index = @section_names.index('.rela.rodata') or raise ArgumentError, 'missing rodata relocations'
    raise ArgumentError, 'wrong relocation symbol table' unless @sections[rel_index][6] == sym_index
    @pointers = {}
    section_data(rel_index).bytes.each_slice(24).each do |bytes|
      offset, info, addend = bytes.pack('C*').unpack('Q<Q<q<')
      next unless (info & 0xffffffff) == 1 # R_X86_64_64
      symbol = @symbols.fetch(info >> 32)
      next unless symbol[:section] == @ro_index
      @pointers[offset] = symbol[:value] + addend
    end
    board = @symbols.find { |s| s[:name] == 'usb_board_vga2usb' } or raise ArgumentError, 'missing original model'
    raise ArgumentError, 'wrong board section' unless board[:section] == @ro_index
    header = pointer(board[:value] + 0x28)
    @version = ro_slice(header + 2, 2).unpack1('v')
    count = ro_slice(header + 0x18, 4).unpack1('V')
    raise ArgumentError, 'wrong original firmware version/count' unless @version == 0x0102 && count == 65
    table = pointer(header + 0x20)
    @segments = count.times.map do |i|
      record = table + i * 24
      address, length, stored, method = ro_slice(record, 16).unpack('V4')
      raise ArgumentError, 'firmware outside original RAM image' unless address + length <= 0x2000 && stored.between?(1, 0x4000)
      data = self.class.decode(method, ro_slice(pointer(record + 16), stored), length)
      {index: i, address: address, length: length, stored: stored, compression: method,
       sha256: Digest::SHA256.hexdigest(data), data: data}
    end
    raise ArgumentError, 'wrong total firmware size' unless @segments.sum { |s| s[:length] } == 7736
  end

  def image
    'V2UF'.b + [0x5555, 0x1110, @version, @segments.size].pack('v4') +
      @segments.map { |s| [s[:address], s[:length]].pack('V2') + s[:data] }.join.b
  end

  private

  def slice(offset, length)
    raise ArgumentError, 'ELF read outside file' unless offset >= 0 && length >= 0 && offset + length <= @data.bytesize
    @data.byteslice(offset, length)
  end

  def section_data(index)
    section = @sections.fetch(index)
    slice(section[4], section[5])
  end

  def ro_slice(offset, length)
    raise ArgumentError, 'firmware read outside rodata' unless offset >= 0 && length >= 0 && offset + length <= @ro.bytesize
    @ro.byteslice(offset, length)
  end

  def pointer(offset)
    @pointers.fetch(offset) { raise ArgumentError, "missing data relocation at 0x#{offset.to_s(16)}" }
  end
end

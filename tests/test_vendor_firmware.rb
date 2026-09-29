require 'minitest/autorun'
require 'zlib'
require 'tmpdir'
require_relative '../tools/vendor_firmware'

class FirmwareDecodeTest < Minitest::Test
  def test_raw_segment_is_exact
    assert_equal "\x01\x02".b, VendorFirmware.decode(0, "\x01\x02".b, 2)
  end

  def test_repeated_pair_counts_additional_bytes_and_resets_pair_state
    packed = [0x12, 0x12, 3, 0x12, 0x34, 0x34, 0, 0x56].pack('C*')
    assert_equal [0x12]*6 + [0x34,0x34,0x56], VendorFirmware.decode(1, packed, 9).bytes
  end

  def test_zlib_segment_accepts_missing_trailer_with_exact_output
    bytes = ('original VGA2USB RAM firmware ' * 20).b
    stream = Zlib::Deflate.deflate(bytes).byteslice(0...-4)
    assert_equal bytes, VendorFirmware.decode(2, stream, bytes.bytesize)
  end

  def test_short_raw_segment_is_rejected
    assert_raises(ArgumentError) { VendorFirmware.decode(0, 'a', 2) }
  end

  def test_truncated_repeat_count_is_rejected
    assert_raises(ArgumentError) { VendorFirmware.decode(1, "\x12\x12".b, 2) }
  end

  def test_compressed_output_cannot_exceed_expected_length
    assert_raises(ArgumentError) { VendorFirmware.decode(2, Zlib::Deflate.deflate('abcdef'), 4) }
  end

  def test_unknown_compression_is_rejected
    assert_raises(ArgumentError) { VendorFirmware.decode(3, 'ab', 2) }
  end

  def test_wrong_vendor_object_is_rejected
    Dir.mktmpdir do |dir|
      path = File.join(dir, 'wrong.o')
      File.binwrite(path, 'not an original VGA2USB vendor object')
      assert_raises(ArgumentError) { VendorFirmware.new(path) }
    end
  end
end

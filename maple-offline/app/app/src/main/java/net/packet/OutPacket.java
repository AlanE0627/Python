package net.packet;

import net.opcodes.SendOpcode;
import android.graphics.Point;

public interface OutPacket extends Packet {
    void writeByte(byte value);
    void writeByte(int value);
    void writeBytes(byte[] value);
    void writeShort(int value);
    void writeInt(int value);
    void writeLong(long value);
    void writeBool(boolean value);
    void writeString(String value);
    void writeFixedString(String value);

    /**
     * maple-offline: fixed-size string field (names: 13 bytes) padded with
     * zeros by BYTES. Padding by characters broke every packet carrying a
     * Hangul name (3 UTF-8 bytes per syllable), e.g. the character entry.
     */
    default void writePaddedString(String value, int length) {
        byte[] src = value == null ? new byte[0] : value.getBytes(constants.string.CharsetConstants.CHARSET);
        int n = Math.min(src.length, length);
        // do not cut a multi-byte character in half
        while (n > 0 && n < src.length && (src[n] & 0xC0) == 0x80) {
            n--;
        }
        byte[] out = new byte[length];
        System.arraycopy(src, 0, out, 0, n);
        writeBytes(out);
    }
    void writePos(Point value);
    void skip(int numberOfBytes);

    static OutPacket create(SendOpcode opcode) {
        return new ByteBufOutPacket(opcode);
    }
}

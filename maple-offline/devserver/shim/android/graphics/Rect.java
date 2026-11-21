package android.graphics;

// desktop stand-in for android.graphics.Rect (maple-offline dev server)
public final class Rect {
    public int left;
    public int top;
    public int right;
    public int bottom;

    public Rect() {}

    public Rect(int left, int top, int right, int bottom) {
        this.left = left; this.top = top; this.right = right; this.bottom = bottom;
    }

    public Rect(Rect r) { this(r.left, r.top, r.right, r.bottom); }

    public final boolean isEmpty() { return left >= right || top >= bottom; }

    public final int width() { return right - left; }

    public final int height() { return bottom - top; }

    public final int centerX() { return (left + right) >> 1; }

    public final int centerY() { return (top + bottom) >> 1; }

    public void set(int left, int top, int right, int bottom) {
        this.left = left; this.top = top; this.right = right; this.bottom = bottom;
    }

    public void set(Rect r) { set(r.left, r.top, r.right, r.bottom); }

    public void offset(int dx, int dy) { left += dx; right += dx; top += dy; bottom += dy; }

    public boolean contains(int x, int y) {
        return left < right && top < bottom && x >= left && x < right && y >= top && y < bottom;
    }

    public boolean contains(Rect r) {
        return left < right && top < bottom && left <= r.left && top <= r.top && right >= r.right && bottom >= r.bottom;
    }

    public boolean intersects(int l, int t, int r, int b) {
        return left < r && l < right && top < b && t < bottom;
    }

    public static boolean intersects(Rect a, Rect b) {
        return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
    }

    public boolean intersect(Rect r) {
        if (left < r.right && r.left < right && top < r.bottom && r.top < bottom) {
            if (left < r.left) left = r.left;
            if (top < r.top) top = r.top;
            if (right > r.right) right = r.right;
            if (bottom > r.bottom) bottom = r.bottom;
            return true;
        }
        return false;
    }

    public void union(Rect r) {
        if (r.left >= r.right || r.top >= r.bottom) return;
        if (left < right && top < bottom) {
            if (left > r.left) left = r.left;
            if (top > r.top) top = r.top;
            if (right < r.right) right = r.right;
            if (bottom < r.bottom) bottom = r.bottom;
        } else {
            set(r);
        }
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof Rect)) return false;
        Rect r = (Rect) o;
        return left == r.left && top == r.top && right == r.right && bottom == r.bottom;
    }

    @Override
    public int hashCode() { return ((left * 31 + top) * 31 + right) * 31 + bottom; }

    @Override
    public String toString() { return "Rect(" + left + ", " + top + " - " + right + ", " + bottom + ")"; }
}

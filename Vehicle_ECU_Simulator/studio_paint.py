"""QPainter implementation of the startup painter interface (model space = the 466 px Torus face)."""
from PySide6.QtCore import Qt, QPointF, QRectF, QLineF
from PySide6.QtGui import QColor, QPen, QPolygonF, QPainterPath
from studio_theme import gray_mask

class QtStartupPainter:
    def __init__(self, p):
        self.p = p
    @staticmethod
    def color(rgb, alpha):
        return QColor(rgb[0], rgb[1], rgb[2], max(0, min(255, round(alpha * 255))))
    def _pen(self, radius, rgb, alpha):
        pen = QPen(self.color(rgb, alpha), max(.3, 2 * radius)); pen.setCapStyle(Qt.RoundCap); pen.setJoinStyle(Qt.RoundJoin)
        return pen
    def line(self, x0, y0, x1, y1, radius, rgb, alpha):
        if alpha <= .002: return
        self.p.setPen(self._pen(radius, rgb, alpha)); self.p.drawLine(QLineF(x0, y0, x1, y1))
    def dot(self, x, y, radius, rgb, alpha):
        if alpha <= .002: return
        self.p.setPen(Qt.NoPen); self.p.setBrush(self.color(rgb, alpha)); r = radius + .5
        self.p.drawEllipse(QPointF(x, y), r, r)
    def stroke(self, points, radius, rgb, alpha):
        if alpha <= .002 or len(points) < 2: return
        path = QPainterPath(QPointF(*points[0]))
        for x, y in points[1:]: path.lineTo(x, y)
        self.p.setPen(self._pen(radius, rgb, alpha)); self.p.setBrush(Qt.NoBrush); self.p.drawPath(path)
    def polygon(self, points, rgb, alpha):
        if alpha <= .002 or len(points) < 3: return
        self.p.setPen(Qt.NoPen); self.p.setBrush(self.color(rgb, alpha))
        self.p.drawPolygon(QPolygonF([QPointF(x, y) for x, y in points]))
    def logo(self, x, y, alpha, clip=None):
        if alpha <= .002: return
        img = gray_mask('assets/preview/halo_logo.png')
        self.p.save(); self.p.setOpacity(alpha)
        if clip: self.p.setClipRect(QRectF(clip[0], y - 2, clip[1] - clip[0], img.height() + 4))
        self.p.drawImage(QRectF(x, y, img.width(), img.height()), img); self.p.restore()

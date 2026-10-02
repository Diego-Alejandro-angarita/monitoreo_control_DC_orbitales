import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "comun"))
import smcd  # noqa: E402


class TestCodificacion(unittest.TestCase):
    def test_reg_node_coincide_con_la_especificacion(self):
        campos = {"node_id": "SAT-01", "intervalo": 5, "tipo": "LEO", "version_sw": "1.0"}
        mensaje = smcd.armar(smcd.REG_NODE, 1, campos)
        self.assertEqual(mensaje[:6], bytes.fromhex("01 01 00 01 00 33"))
        self.assertEqual(mensaje[6:], b"node_id=SAT-01;intervalo=5;tipo=LEO;version_sw=1.0;")

    def test_ida_y_vuelta(self):
        campos = {"status": "OK", "detail": "temp_int 92.1", "vacio": ""}
        tipo, seq, leidos = smcd.desarmar(smcd.armar(smcd.EVENT_ACK, 65535, campos))
        self.assertEqual((tipo, seq, leidos), (smcd.EVENT_ACK, 65535, campos))

    def test_payload_vacio(self):
        self.assertEqual(smcd.decodificar_payload(b""), {})

    def test_valor_con_separadores_no_se_codifica(self):
        with self.assertRaises(ValueError):
            smcd.codificar_payload({"pass": "a;b"})
        with self.assertRaises(ValueError):
            smcd.codificar_payload({"pass": "a=b"})

    def test_payload_demasiado_grande(self):
        with self.assertRaises(ValueError):
            smcd.armar(smcd.STATE_REPORT, 1, {f"k{i}": "x" * 100 for i in range(10)}, smcd.MAX_PAYLOAD_UDP)


class TestDecodificacionInvalida(unittest.TestCase):
    def test_payloads_invalidos(self):
        for datos in [b"a=1", b"a=1;a=2;", b"a=b=c;", b"=1;", b"a=1;;", b"\xff=1;", b"x" * 33 + b"=1;"]:
            with self.subTest(datos=datos), self.assertRaises(smcd.ErrorProtocolo):
                smcd.decodificar_payload(datos)

    def test_datagrama_corto(self):
        with self.assertRaises(smcd.ErrorProtocolo):
            smcd.desarmar(b"\x01\x05")

    def test_version_incorrecta(self):
        with self.assertRaises(smcd.ErrorProtocolo):
            smcd.desarmar(bytes.fromhex("02 05 00 01 00 00"))

    def test_longitud_inconsistente(self):
        with self.assertRaises(smcd.ErrorProtocolo):
            smcd.desarmar(bytes.fromhex("01 05 00 01 00 05") + b"a=1;")


class TestUtilidades(unittest.TestCase):
    def test_siguiente_seq_da_la_vuelta(self):
        self.assertEqual(smcd.siguiente_seq(0xFFFF), 0)
        self.assertEqual(smcd.siguiente_seq(41), 42)

    def test_describir_error(self):
        texto = smcd.describir_error({"code": "5", "msg": "nodo desconocido"})
        self.assertEqual(texto, "UNKNOWN_NODE (5): nodo desconocido")


if __name__ == "__main__":
    unittest.main()

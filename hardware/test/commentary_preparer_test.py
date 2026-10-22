"""Solo clasificación: sin FFmpeg, puerto ni escritura de originales."""
from pathlib import Path
import importlib.util
import sys
sys.dont_write_bytecode=True
import unittest
script=Path(__file__).resolve().parents[2]/"scripts/preparar-comentarios.py"
spec=importlib.util.spec_from_file_location("preparer",script)
preparer=importlib.util.module_from_spec(spec);spec.loader.exec_module(preparer)
class Classification(unittest.TestCase):
    def test_legacy(self):
        self.assertEqual(preparer.classify("GOL_1_AFTER INVITING PRESSURE_0_0.wav"),(1,0,0))
        self.assertEqual(preparer.classify("GOL_6_COMODIN 3_19_19.wav"),(6,19,19))
        self.assertEqual(preparer.classify("GOL_EMPATE TRAS ULTIMO MINUTO CON MAS DE 2 GOLES_5_5.wav"),(7,5,5))
    def test_losing_and_lead(self):
        self.assertEqual(preparer.classify("GOL_ GOL PERO EL EQUIPÒ SIGUE PERDIENDO_2_2.wav"),(8,2,2))
        self.assertEqual(preparer.classify("GOL_ GOL Y EL EQUIPO VA GANANDO DE MAS DE 2 GOLES_1_1.wav"),(9,1,1))
        self.assertEqual(preparer.GROUPS[8][0],6);self.assertEqual(preparer.GROUPS[9][0],7)
    def test_wildcards_and_distinct_ids(self):
        for batch in range(3,6):
            self.assertEqual(preparer.classify(f"GOL COMODIN {batch}_0_0.wav"),(batch+7,0,0))
            self.assertEqual(preparer.GROUPS[batch+7][0],0)
        self.assertNotEqual(preparer.GROUPS[6][1],preparer.GROUPS[10][1])
    def test_more_eight_and_reserved_tournament(self):
        self.assertEqual(preparer.classify("GOL_EQUIPO QUE HA MARCADO LLEVA MAS DE 8 GOLES_0_0.wav"),(14,0,0))
        self.assertEqual(preparer.GROUPS[14][0],8)
        self.assertEqual(preparer.classify("GOL_ EQUIPO QUE MARCA EL GOL SIGUE PERDIENDO_0_0.wav"),(16,0,0))
        self.assertEqual(preparer.GROUPS[16][0],6)
        self.assertEqual(preparer.classify("GOL_EQUIPO QUE RECIBE EL GOL NECESITA 2 GOLES PARA PASAR DE RONDA_1_1.wav"),(13,1,1))
        self.assertEqual(preparer.classify("GOL_ SOLO EN TORNEOS, EQUIPO LLEVA MAS DE 8 GOLES EN EL PARTIDO_2_2.wav"),(15,2,2))
        self.assertIsNone(preparer.GROUPS[13][0]);self.assertIsNone(preparer.GROUPS[15][0])
    def test_final_tournament_reserved(self):
        for group,prefix in ((17,"GOL EMPATE"),(18,"GOL EMPATE ULTUIMO SEGUNDO FINAL"),(19,"GOL FINAL GANANDO ULTIMOS MINUTOS"),(20,"GOL FINAL CON VENTAJA")):
            self.assertEqual(preparer.classify(prefix+"_0_0.wav"),(group,0,0))
            self.assertIsNone(preparer.GROUPS[group][0])
    def test_unknown_not_guessed(self):
        for name in ("GOL ALGO NUEVO_0_0.wav","GOL COMODIN 6_0_0.wav","GOL_7_OTRO_0_0.wav"):
            self.assertIsNone(preparer.classify(name))
if __name__=="__main__":unittest.main()

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
    def test_new_batch(self):
        for group,prefix in ((21,"GOL FINAL Y VAN GANANDO"),(22,"GOL GANANDO DE MAS DE 1 EN LA FINAL"),(23,"GOL COMODIN FINAL IR GANANDO"),(24,"GOL FINAL GANANDO ULTIMOS 20 SEG"),(25,"GOL Y GANANDO 2"),(26,"GOL  Y GANANDO 1"),(27,"GOL ULTIMO SEG 2"),(28,"GOL ULTIMOS SEG 1"),(29,"GOL EMPATE 2"),(30,"GOL COMODIN 6")):
            self.assertEqual(preparer.classify(prefix+"_0_0.wav"),(group,0,0))
        for group in range(21,25): self.assertIsNone(preparer.GROUPS[group][0])
        self.assertEqual([preparer.GROUPS[g][0] for g in (25,26,27,28,29,30,31)],[9,9,10,10,2,0,2])
        self.assertNotEqual(preparer.GROUPS[17][1],preparer.GROUPS[31][1])
        # El nombre repetido conserva su significado histórico sin asignación explícita.
        self.assertEqual(preparer.classify("GOL EMPATE_0_0.wav"),(17,0,0))
    def test_bad_overrides(self):
        for value in ([], {"GOL EMPATE_0_0.wav":999}, {"GOL EMPATE_0_0.wav":True}, {"ausente":31}):
            with self.assertRaises(ValueError): preparer.prepare(Path("__no_sources__"),Path("__no_output__"),"__no_ffmpeg__",value)
    def test_events_and_contexts(self):
        rows=((32,"FINAL DEL PARTIDO SIN PRORROGA"),(33,"INTRO, REPRODUCIR MIENTRAS LA CUENTA ATRAS DE 3 SEGUNDOS ANTES DEL PARTIDO"),(34,"FINAL DE LA 1º PARTE"),(35,"GOL TORNEO AL EQUIPO QUE MARCA LE FALTA 1 GOL PARA IGUALAR LA ELIMINATORIA Y QUEDA MENOS DE 30 SEGUNDOS"),(36,"GOL TORNEO AL EQUIPO QUE MARCA LE FALTA 1 GOL PARA IGUALAR LA ELIMINATORIA"),(37,"GOL REMONTADA 1"),(38,"GOL CUANDOI EMPATAS DESPUES DE HABER IDO PERDIENDO DE 3 O MAS"),(39,"GOL ULTIMO MINUTO Y PIERDE DE MAS DE 3"),(40,"GOL PERO EL EQUIPO PIERDE DE MAS DE 3"),(41,"GOL COMODIN ULTI 20 SEG"),(42,"GOL COMODIN 7"))
        for group,prefix in rows: self.assertEqual(preparer.classify(prefix+"_0_0.wav"),(group,0,0))
        for group in (35,36): self.assertIsNone(preparer.GROUPS[group][0])
        self.assertEqual([preparer.GROUPS[g][0] for g in (32,33,34,37,38,39,40,41,42)],[13,11,12,14,15,16,17,10,0])
    def test_penalties_and_ambient(self):
        for group,prefix in ((43,"PENALTY FALLADO"),(44,"GOL DE PENALTY Y EMPATAS EN LOS ULTIMOS 20 SEG"),(45,"GOL DE PENALETY Y EMPATAS"),(46,"GOL DE PENALTY"),(47,"SONIDOS RAMDOM CUALQUIER MOMENTO DEL PARTIDO 3"),(48,"SONIDOS RAMDOM CUALQUIER MOMENTO DEL PARTIDO 2"),(49,"SONIDOS RAMDOM CUALQUIER MOMENTO DEL PARTIDO"),(50,"SONIDOS PARA DESPUES DEL PITIDO INICIAL, EN LOS PRIMEROS  20 SEG DE PARTIDO, COMODIN")):
            self.assertEqual(preparer.classify(prefix+"_0_0.wav"),(group,0,0))
        self.assertEqual([preparer.GROUPS[g][0] for g in range(43,51)],[18,21,20,19,22,22,22,23])
    def test_unknown_not_guessed(self):
        for name in ("GOL ALGO NUEVO_0_0.wav","GOL COMODIN 8_0_0.wav","GOL_7_OTRO_0_0.wav"):
            self.assertIsNone(preparer.classify(name))
if __name__=="__main__":unittest.main()

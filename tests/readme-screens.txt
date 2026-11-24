wait 600
tap ENV
turn SELECT 1
expect operator 1
shot operator-envelope.png
tap HOME
tap REC
midi 90 3C 64
wait 400
midi 80 3C 00
wait 200
tap REC
wait 500
tap PLAY
turn K2 6
expect quantize 6
wait 250
shot looper.png

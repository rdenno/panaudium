import sys, numpy as np, soundfile as sf
for f in sys.argv[1:]:
    y, sr = sf.read(f); m = y.mean(axis=1)[int(0.5*sr):int(1.4*sr)]
    n = int(f.split('_n')[1].split('.')[0]); f0 = 440*2**((n-69)/12)
    S = np.abs(np.fft.rfft(m*np.hanning(len(m)))); fr = np.fft.rfftfreq(len(m), 1/sr)
    def amp(h): 
        idx = (fr > f0*h*0.97) & (fr < f0*h*1.03); return 20*np.log10(S[idx].max()+1e-9)
    print(f.split('\\')[-1], ' '.join(f"{amp(h):6.1f}" for h in [0.5,1,1.5,2,3,4,5]))

import numpy as np
from pyscf import gto, scf, ao2mo, mcscf, tools, fci, mp
from pyscf import lo
from pyscf.lo import pipek, boys, edmiston, iao, ibo
from scipy.linalg import fractional_matrix_power
import scipy.linalg as la

def doRHF(mol):
  mf = scf.RHF(mol)
  print(mf.kernel())
  return mf

#it may be necessary (more often than not) to provide a system curated initial guess for the dm rather than just adding noise
#if using noise, changing its magnitude may change the final answer as well
def doUHF(mol, dm=None):
  umf = scf.UHF(mol)
  if dm is None:
    dm = umf.get_init_guess()
    norb = mol.nao
    dm[0] = dm[0] + np.random.rand(norb, norb) / 2
    dm[1] = dm[1] + np.random.rand(norb, norb) / 2
  print(umf.kernel(dm0 = dm))
  return umf

#it may be necessary (more often than not) to provide a system curated initial guess for the dm rather than just adding noise
#if using noise, changing its magnitude may change the final answer as well
def doGHF(mol, dm=None):
  gmf = scf.GHF(mol)
  gmf.max_cycle = 200
  if dm is None:
    dm = gmf.get_init_guess()
    norb = mol.nao
    dm = dm + np.random.rand(2*norb, 2*norb) / 3
  print(gmf.kernel(dm0 = dm))
  return gmf

def localizeAllElectron(mf, method="lowdin"):
  if (method == "lowdin"):
    return fractional_matrix_power(mf.get_ovlp(), -0.5).T
  elif (method == "pm"):
    return pipek.PM(mf.mol).kernel(mf.mo_coeff)
  elif (method == "boys"):
    return boys.Boys(mf.mol).kernel(mf.mo_coeff)
  elif (method == "er"):
    return edmiston.ER(mf.mol).kernel(mf.mo_coeff)
  elif (method == "iao"):
    return iao.iao(mf.mol, mf.mo_coeff)
  elif (method == "ibo"):
    a = iao.iao(mf.mol, mf.mo_coeff)
    a = lo.vec_lowdin(a, mf.get_ovlp())
    return ibo.ibo(mf.mol, mf.mo_coeff, iaos=a)

def localizeValence(mf, mo_coeff, method="iao"):
  if (method == "iao"):
    return iao.iao(mf.mol, mo_coeff)
  elif (method == "ibo"):
    a = iao.iao(mf.mol, mo_coeff)
    a = lo.vec_lowdin(a, mf.get_ovlp())
    return ibo.ibo(mf.mol, mo_coeff, iaos=a)
  elif (method == "boys"):
    return boys.Boys(mf.mol).kernel(mo_coeff)
  elif (method == "er"):
    return edmiston.ER(mf.mol).kernel(mo_coeff)

# can be used for all electron, but not recommended
def bestDetValence(mol, lmo, occ, eri, writeToFile=True):
  maxLMOContributers = [ np.argmax(np.abs(lmo[::,i])) for i in range(lmo.shape[1]) ]  # index of the ao contributing the most to an lmo
  atomNumAOs = [ i[1][3] - 1 for i in enumerate(mol.aoslice_nr_by_atom()) ]  # end AO index for each atom in ascending order
  lmoSites = [ [] for i in range(mol.natm) ] #lmo's cetered on each atom
  for i in enumerate(maxLMOContributers):
    lmoSites[np.searchsorted(np.array(atomNumAOs), i[1])].append(i[0])

  bestDet = [0 for i in range(lmo.shape[1])]
  def pair(i):
    return i*(i+1)//2+i
  for i in enumerate(occ):
    if eri.ndim == 2:
      onSiteIntegrals = [ (j, eri[pair(j),pair(j)]) for (n,j) in enumerate(lmoSites[i[0]]) ]
    elif eri.ndim == 1:
      onSiteIntegrals = [ (j, eri[pair(pair(j))]) for (n,j) in enumerate(lmoSites[i[0]]) ]
    onSiteIntegrals.sort(key = lambda tup : tup[1], reverse=True)
    for k in range(i[1][0]):
      bestDet[onSiteIntegrals[k][0]] = '2'
    for l in range(i[1][1]):
      bestDet[onSiteIntegrals[i[1][0] + l][0]] = 'a'
    for m in range(i[1][2]):
      bestDet[onSiteIntegrals[i[1][0] + i[1][1] + m][0]] = 'b'

  bestDetStr = '  '.join(bestDet)
  print('bestDet:  ' + bestDetStr)
  if writeToFile:
    fileh = open("bestDet", 'w')
    fileh.write('1.   ' + bestDetStr + '\n')
    fileh.close()

  return bestDetStr

def writeFCIDUMP(mol, mf, lmo):
  h1 = lmo.T.dot(mf.get_hcore()).dot(lmo)
  eri = ao2mo.kernel(mol, lmo)
  tools.fcidump.from_integrals('FCIDUMP', h1, eri, mol.nao, mol.nelectron, mf.energy_nuc())

def basisChange(matAO, lmo, ovlp):
  matMO = (matAO.T.dot(ovlp).dot(lmo)).T
  return matMO

def writeMat(mat, fileName, isComplex):
  fileh = open(fileName, 'w')
  for i in range(mat.shape[0]):
      for j in range(mat.shape[1]):
        if (isComplex):
          fileh.write('(%16.10e, %16.10e) '%(mat[i,j].real, mat[i,j].imag))
        else:
          fileh.write('%16.10e '%(mat[i,j]))
      fileh.write('\n')
  fileh.close()

def readMat(fileName, shape, isComplex):
  if(isComplex):
    matr = np.zeros(shape)
    mati = np.zeros(shape)
  else:
    mat = np.zeros(shape)
  row = 0
  fileh = open(fileName, 'r')
  for line in fileh:
    col = 0
    for coeff in line.split():
      if (isComplex):
        m = coeff.strip()[1:-1]
        matr[row, col], mati[row, col] = [float(x) for x in m.split(',')]
      else:
        mat[row, col]  = float(coeff)
      col = col + 1
    row = row + 1
  fileh.close()
  if (isComplex):
    mat = matr + 1j * mati
  return mat

def makeAGPFromRHF(rhfCoeffs):
  norb = rhfCoeffs.shape[0]
  nelec = 2*rhfCoeffs.shape[1]
  diag = np.eye(nelec//2)
  #diag = np.zeros((norb,norb))
  #for i in range(nelec/2):
  #  diag[i,i] = 1.
  pairMat = rhfCoeffs.dot(diag).dot(rhfCoeffs.T)
  return pairMat

def makePfaffFromGHF(ghfCoeffs):
  nelec = ghfCoeffs.shape[1]
  amat = np.full((nelec, nelec), 0.)
  for i in range(nelec//2):
    amat[2 * i + 1, 2 * i] = -1.
    amat[2 * i, 2 * i + 1] = 1.
  pairMat = ghfCoeffs.dot(amat).dot(ghfCoeffs.T)
  return pairMat

def addNoise(mat, isComplex):
  if (isComplex):
    randMat = 0.01 * (np.random.rand(mat.shape[0], mat.shape[1]) + 1j * np.random.rand(mat.shape[0], mat.shape[1]))
    return mat + randMat
  else:
    randMat = 0.01 * np.random.rand(mat.shape[0], mat.shape[1])
    return mat + randMat

def prepAllElectron(mol, loc="lowdin", dm=None, writeFcidump=True, writeMOs=True):
  mf = doRHF(mol)
  lmo = localizeAllElectron(mf, loc)
  if writeFcidump:
    writeFCIDUMP(mol, mf, lmo)
  gmf = doGHF(mol, dm)
  overlap = mf.get_ovlp(mol)
  ghfCoeffs = basisChange(gmf.mo_coeff, la.block_diag(lmo, lmo), la.block_diag(overlap, overlap))
  if writeMOs:
    writeMat(ghfCoeffs, "hf.txt", False)

if __name__=="__main__":
  r = 2.0
  n = 10
  order = 2
  sqrt2 = 2**0.5
  
  atomstring = ""
  for i in range(n):
      atomstring += "H 0 0 %g\n"%(i*r)
  
  mol = gto.M(
      atom = atomstring,
      unit = 'Bohr',
      basis = 'sto-6g',
      verbose=4,
      symmetry=0,
      spin = 0)
  mf = scf.RHF(mol)
  print(mf.kernel())

  lmo = localizeAllElectron(mf, "lowdin")

  writeFCIDUMP(mol, mf, lmo)

  # Perform GHF and print these orbitals to hf.txt
  gmf = doGHF(mol)
  overlap = mf.get_ovlp(mol)
  ghfCoeffs = basisChange(gmf.mo_coeff, la.block_diag(lmo, lmo), la.block_diag(overlap, overlap))
  writeMat(ghfCoeffs, "hf.txt", False)

#include "ProofVisitor.h"
#include "SolverTypes.h"


namespace Minisat
{
  int TraceProofVisitor::visitResolvent (Lit parent, Lit p1, CRef p2)
  {
    if (!m_units [var (p1)])
    {
      m_units [var (p1)] = true;
      printf ("vL (l%d)\n", toInt (p1));
    }
    bool v;
    if (!m_visited.has (p2, v))
    {
      m_visited.insert (p2, true);
      printf ("vL (c%d)\n", p2);
    }
    
    printf ("vR (l%d, l%d, c%d)\n", toInt (parent), toInt (p1), p2);
    return 0;
  }

  int TraceProofVisitor::visitHyperResolvent (Lit parent)
  {
    doAntecendents ();
    Var vp = var (parent);
    
    m_units [vp] = true;
    
    printf ("vH (l%d 0 ", toInt (parent));

    printf ("c%d ", hyperClauses [0]);
    for (int i = 0; i < hyperPivots.size (); ++i)
    {
      if (i+1 < hyperClauses.size ())
        printf ("c%d ", hyperClauses [i+1]);
      else
        printf ("l%d ", toInt (hyperPivots[i]));
    }
    printf (" 0)\n");

    return 0;
  }

  void TraceProofVisitor::doAntecendents ()
  {   
    bool v;
    if (!m_visited.has (hyperClauses [0], v))
    {
      m_visited.insert (hyperClauses [0], true);
      printf ("vL (c%d)\n", hyperClauses [0]);
    }
    
    for (int i = 0; i < hyperPivots.size (); ++i)
    {
      if (i + 1 < hyperClauses.size ())
      {
        if (!m_visited.has (hyperClauses [i], v))
        {
          m_visited.insert (hyperClauses [i], true);
          printf ("vL (c%d)\n", hyperClauses [i]);
        }
      }
      else
      {
        Var vp = var (hyperPivots [i]);
        if (!m_units [vp])
        {
          m_units [vp] = true;
          printf ("vL (l%d)\n", toInt (hyperPivots [i]));
        }
      }
    }
  }
  
  int TraceProofVisitor::visitHyperResolvent (CRef parent)
  {
    m_visited.insert (parent, true);
    doAntecendents ();
    
    printf ("vH (c%d 0 ", toInt (parent));
    
    printf ("c%d ", hyperClauses [0]);
    for (int i = 0; i < hyperPivots.size (); ++i)
    {
      if (i+1 < hyperClauses.size ())
        printf ("c%d ", hyperClauses [i+1]);
      else
        printf ("l%d ", toInt (hyperPivots[i]));
    }
    printf (" 0)\n");
    return 0;
  }
}




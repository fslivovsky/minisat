#include "ProofVisitor.h"
#include "SolverTypes.h"


namespace Minisat
{
  namespace
  {
    void toDimacs (Lit lit)
    {
      printf ("%s%d", sign (lit) ? "-" : "", var (lit) + 1);
    }
    
    void toDimacs (const Clause &c)
    {
      for (int i = 0; i < c.size (); ++i)
      {
        toDimacs (c[i]);
        printf (" ");
      }
    }
    
  }
  
  int TraceProofVisitor::visitResolvent (Lit parent, Lit p1, CRef p2)
  {
    if (m_units [var (p1)] < 0)
    {
      m_units [var (p1)] = m_ids++;
      printf ("%d ", m_ids - 1);
      toDimacs (p1);
      printf (" 0 0\n");
    }
    int id;
    if (!m_visited.has (p2, id))
    {
      m_visited.insert (p2, m_ids++);
      printf ("%d ", m_ids - 1);
      toDimacs (m_Solver.getClause (p2));
      printf (" 0 0\n");
    }
    
    m_units [var (parent)] = m_ids++;
    
    printf ("%d ", m_ids - 1);
    toDimacs (parent);
    printf (" 0 ");
    
    id = -1;
    m_visited.has (p2, id);
    printf ("%d %d 0\n", m_units [var (p1)], id);
    
    return 0;
  }

  int TraceProofVisitor::visitChainResolvent (Lit parent)
  {
    doAntecendents ();
    Var vp = var (parent);
    
    m_units [vp] = m_ids++;
    printf ("%d ", m_ids-1);
    toDimacs (parent);
    printf (" 0 ");

    int id;
    m_visited.has (chainClauses [0], id);
    printf ("%d ", id);
    for (int i = 0; i < chainPivots.size (); ++i)
    {
      if (i+1 < chainClauses.size ())
      {
        m_visited.has (chainClauses [i+1], id);
        printf ("%d ", id);
      }
      else
        printf ("%d ", m_units [var (chainPivots [i])]);
    }
    printf (" 0\n");

    return 0;
  }

  void TraceProofVisitor::doAntecendents ()
  {   
    int id;
    if (!m_visited.has (chainClauses [0], id))
    {
      m_visited.insert (chainClauses [0], m_ids++);
      printf ("%d ", m_ids-1);
      toDimacs (m_Solver.getClause (chainClauses [0]));
      printf (" 0 0\n");
    }
    
    for (int i = 0; i < chainPivots.size (); ++i)
    {
      if (i + 1 < chainClauses.size ())
      {
        if (!m_visited.has (chainClauses [i+1], id))
        {
          m_visited.insert (chainClauses [i+1], m_ids++);
          printf ("%d ", m_ids-1);
          toDimacs (m_Solver.getClause (chainClauses [i+1]));
          printf (" 0 0\n");
        }
      }
      else
      {
        Var vp = var (chainPivots [i]);
        if (m_units [vp] < 0)
        {
          m_units [vp] = m_ids++;
          printf ("%d ", m_ids-1);
          toDimacs (chainPivots [i]);
          printf (" 0 0\n");
        }
      }
    }
  }
  
  int TraceProofVisitor::visitChainResolvent (CRef parent)
  {
    doAntecendents ();
    
    if (parent != CRef_Undef)
      m_visited.insert (parent, m_ids++);
    else m_ids++;
    
    printf ("%d ", m_ids-1);
    if (parent != CRef_Undef) toDimacs (m_Solver.getClause (parent));
    printf (" 0 ");
    
    int id;
    m_visited.has (chainClauses [0], id);
    printf ("%d ", id);
    for (int i = 0; i < chainPivots.size (); ++i)
    {
      if (i+1 < chainClauses.size ())
      {
        m_visited.has (chainClauses [i+1], id);
        printf ("%d ", id);
      }
      else
        printf ("%d ", m_units [var (chainPivots [i])]);
    }
    printf (" 0\n");
    return 0;
  }
}




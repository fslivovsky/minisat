#include "TraceProofVisitor.h"

namespace Minisat
{
  namespace
  {
    void toDimacs (FILE* out, Lit lit)
    {
      fprintf (out, "%s%d", sign (lit) ? "-" : "", var (lit) + 1);
    }
    
    void toDimacs (FILE* out, const Clause &c)
    {
      for (int i = 0; i < c.size (); ++i)
      {
        toDimacs (out, c[i]);
        fprintf (out, " ");
      }
    }
    
  }
    
  TraceProofVisitor::TraceProofVisitor (Solver &solver, FILE* out) 
    : m_Solver (solver), m_ids(1), m_out (out) 
   {
     m_units.growTo (m_Solver.nVars (), -1);
   }
 
  int TraceProofVisitor::visitResolvent (Lit parent, Lit p1, CRef p2)
  {
    if (m_units [var (p1)] < 0)
    {
      m_units [var (p1)] = m_ids++;
      fprintf (m_out, "%d ", m_ids - 1);
      toDimacs (m_out, p1);
      fprintf (m_out, " 0 0\n");
    }
    int id;
    if (!m_visited.has (p2, id))
    {
      m_visited.insert (p2, m_ids++);
      fprintf (m_out, "%d ", m_ids - 1);
      toDimacs (m_out, m_Solver.getClause (p2));
      fprintf (m_out, " 0 0\n");
    }
    
    m_units [var (parent)] = m_ids++;
    
    fprintf (m_out, "%d ", m_ids - 1);
    toDimacs (m_out, parent);
    fprintf (m_out, " 0 ");
    
    id = -1;
    m_visited.has (p2, id);
    fprintf (m_out, "%d %d 0\n", m_units [var (p1)], id);
    
    return 0;
  }

  int TraceProofVisitor::visitChainResolvent (Lit parent)
  {
    doAntecendents ();
    Var vp = var (parent);
    
    m_units [vp] = m_ids++;
    fprintf (m_out, "%d ", m_ids-1);
    toDimacs (m_out, parent);
    fprintf (m_out, " 0 ");

    int id;
    m_visited.has (chainClauses [0], id);
    fprintf (m_out, "%d ", id);
    for (int i = 0; i < chainPivots.size (); ++i)
    {
      if (i+1 < chainClauses.size () && chainClauses [i+1] != CRef_Undef)
      {
        m_visited.has (chainClauses [i+1], id);
        fprintf (m_out, "%d ", id);
      }
      else
        fprintf (m_out, "%d ", m_units [var (chainPivots [i])]);
    }
    fprintf (m_out, " 0\n");

    return 0;
  }

  void TraceProofVisitor::doAntecendents ()
  {   
    int id;
    if (!m_visited.has (chainClauses [0], id))
    {
      m_visited.insert (chainClauses [0], m_ids++);
      fprintf (m_out, "%d ", m_ids-1);
      toDimacs (m_out, m_Solver.getClause (chainClauses [0]));
      fprintf (m_out, " 0 0\n");
    }
    
    for (int i = 0; i < chainPivots.size (); ++i)
    {
      if (i + 1 < chainClauses.size () && chainClauses[i+1] != CRef_Undef)
      {
        if (!m_visited.has (chainClauses [i+1], id))
        {
          m_visited.insert (chainClauses [i+1], m_ids++);
          fprintf (m_out, "%d ", m_ids-1);
          toDimacs (m_out, m_Solver.getClause (chainClauses [i+1]));
          fprintf (m_out, " 0 0\n");
        }
      }
      else
      {
        Var vp = var (chainPivots [i]);
        if (m_units [vp] < 0)
        {
          m_units [vp] = m_ids++;
          fprintf (m_out, "%d ", m_ids-1);
          toDimacs (m_out, chainPivots [i]);
          fprintf (m_out, " 0 0\n");
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
    
    fprintf (m_out, "%d ", m_ids-1);
    if (parent != CRef_Undef) toDimacs (m_out, m_Solver.getClause (parent));
    fprintf (m_out, " 0 ");
    
    int id;
    m_visited.has (chainClauses [0], id);
    fprintf (m_out, "%d ", id);
    for (int i = 0; i < chainPivots.size (); ++i)
    {
      if (i+1 < chainClauses.size () && chainClauses[i+1] != CRef_Undef)
      {
        m_visited.has (chainClauses [i+1], id);
        fprintf (m_out, "%d ", id);
      }
      else
        fprintf (m_out, "%d ", m_units [var (chainPivots [i])]);
    }
    fprintf (m_out, " 0\n");
    return 0;
  }
}




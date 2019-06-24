#include <limits>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include "../utils/bitSet.hpp"
#include "undirectedGraph.hpp"
#include"outlierScore.hpp"
#include"cluster.hpp"
#include"hdbscanConstraint.hpp"

namespace hdbscanStar
{

	class hdbscanAlgorithm
	{
	public:
		/// <summary>
        /// Calculates the core distances for each point in the data set, given some value for k.
        /// </summary>
        /// <param name="distances">A vector of vectors where index [i][j] indicates the jth attribute of data point i</param>
        /// <param name="k">Each point's core distance will be it's distance to the kth nearest neighbor</param>
        /// <returns> An array of core distances</returns>
		static std::vector<double> calculateCoreDistances(std::vector<std::vector<double>> distances, int k)
		{
			int length = distances.size();

			int numNeighbors = k - 1;
			std::vector<double>coreDistances(length);
			if (k == 1)
			{
				for (int point = 0; point < length; point++)
				{
					coreDistances[point] = 0;
				}
				return coreDistances;
			}
			for (int point = 0; point < length; point++)
			{
				std::vector<double> kNNDistances(numNeighbors);  //Sorted nearest distances found so far
				for (int i = 0; i < numNeighbors; i++)
				{
					kNNDistances[i] = std::numeric_limits<double>::max();
				}

				for (int neighbor = 0; neighbor < length; neighbor++)
				{
					if (point == neighbor)
						continue;
					double distance = distances[point][neighbor];
					int neighborIndex = numNeighbors;
					//Check at which position in the nearest distances the current distance would fit:
					while (neighborIndex >= 1 && distance < kNNDistances[neighborIndex - 1])
					{
						neighborIndex--;
					}
					//Shift elements in the array to make room for the current distance:
					if (neighborIndex < numNeighbors)
					{
						for (int shiftIndex = numNeighbors - 1; shiftIndex > neighborIndex; shiftIndex--)
						{
							kNNDistances[shiftIndex] = kNNDistances[shiftIndex - 1];
						}
						kNNDistances[neighborIndex] = distance;
					}

				}
				coreDistances[point] = kNNDistances[numNeighbors - 1];
			}
			return coreDistances;
		}
		static undirectedGraph constructMst(std::vector<std::vector<double>> distances, std::vector<double> coreDistances, bool selfEdges)
		{
			int length = distances.size();
			int selfEdgeCapacity = 0;
			if (selfEdges)
				selfEdgeCapacity = length;
			bitSet attachedPoints;

			std::vector<int> nearestMRDNeighbors(length - 1 + selfEdgeCapacity);
			std::vector<double> nearestMRDDistances(length - 1 + selfEdgeCapacity);

			for (int i = 0; i < length - 1; i++)
			{
				nearestMRDDistances[i] = std::numeric_limits<double>::max();
			}

			int currentPoint = length - 1;
			int numAttachedPoints = 1;
			attachedPoints.set(length - 1);

			while (numAttachedPoints < length)
			{

				int nearestMRDPoint = -1;
				double nearestMRDDistance = std::numeric_limits<double>::max();
				for (int neighbor = 0; neighbor < length; neighbor++)
				{
					if (currentPoint == neighbor)
						continue;
					if (attachedPoints.get(neighbor) == true)
						continue;
					double distance = distances[currentPoint][neighbor];
					double mutualReachabiltiyDistance = distance;
					if (coreDistances[currentPoint] > mutualReachabiltiyDistance)
						mutualReachabiltiyDistance = coreDistances[currentPoint];

					if (coreDistances[neighbor] > mutualReachabiltiyDistance)
						mutualReachabiltiyDistance = coreDistances[neighbor];

					if (mutualReachabiltiyDistance < nearestMRDDistances[neighbor])
					{
						nearestMRDDistances[neighbor] = mutualReachabiltiyDistance;
						nearestMRDNeighbors[neighbor] = currentPoint;
					}

					if (nearestMRDDistances[neighbor] <= nearestMRDDistance)
					{
						nearestMRDDistance = nearestMRDDistances[neighbor];
						nearestMRDPoint = neighbor;
					}

				}
				attachedPoints.set(nearestMRDPoint);
				numAttachedPoints++;
				currentPoint = nearestMRDPoint;
			}
			std::vector<int> otherVertexIndices(length - 1 + selfEdgeCapacity);
			for (int i = 0; i < length - 1; i++)
			{
				otherVertexIndices[i] = i;
			}
			if (selfEdges)
			{
				for (int i = length - 1; i < length * 2 - 1; i++)
				{
					int vertex = i - (length - 1);
					nearestMRDNeighbors[i] = vertex;
					otherVertexIndices[i] = vertex;
					nearestMRDDistances[i] = coreDistances[vertex];
				}
			}
			undirectedGraph undirectedGraphObject(length, nearestMRDNeighbors, otherVertexIndices, nearestMRDDistances);
			return undirectedGraphObject;

		}
		/// <summary>
		/// Propagates constraint satisfaction, stability, and lowest child death level from each child
		/// cluster to each parent cluster in the tree.  This method must be called before calling
		/// findProminentClusters() or calculateOutlierScores().
		/// </summary>
		/// <param name="clusters">A list of Clusters forming a cluster tree</param>
		/// <returns>true if there are any clusters with infinite stability, false otherwise</returns>

		static bool propagateTree(std::vector<cluster> clusters)
		{
			std::map<int, cluster> clustersToExamine;
			bitSet addedToExaminationList;
			bool infiniteStability = false;

			//Find all leaf clusters in the cluster tree:
			for(cluster cluster : clusters)
			{
				if (/*cluster != NULL && */!cluster.HasChildren)
				{
					int label = cluster.Label;
					clustersToExamine.erase(label);
					clustersToExamine.insert({ label, cluster });
					addedToExaminationList.set(label);
				}
			}

			//Iterate through every cluster, propagating stability from children to parents:
			while (clustersToExamine.size())
			{
				std::map<int, cluster>::iterator currentKeyValue = clustersToExamine.end();
				cluster currentCluster = currentKeyValue->second;
				clustersToExamine.erase(currentKeyValue->first);

				currentCluster.propagate();

				if (currentCluster.Stability == std::numeric_limits<double>::infinity())
					infiniteStability = true;

				if (currentCluster.Parent != NULL)
				{
					cluster *parent = currentCluster.Parent;
					int label = parent->Label;

					if (!addedToExaminationList.get(label))
					{
						clustersToExamine.erase(label);
						clustersToExamine.insert({ label, *parent });
						addedToExaminationList.set(label);
					}
				}
			}

			return infiniteStability;
		}

		/// <summary>
		/// Produces the outlier score for each point in the data set, and returns a sorted list of outlier
		/// scores.  propagateTree() must be called before calling this method.
		/// </summary>
		/// <param name="clusters">A list of Clusters forming a cluster tree which has already been propagated</param>
		/// <param name="pointNoiseLevels">A double[] with the levels at which each point became noise</param>
		/// <param name="pointLastClusters">An int[] with the last label each point had before becoming noise</param>
		/// <param name="coreDistances">An array of core distances for each data point</param>
		/// <returns>An List of OutlierScores, sorted in descending order</returns>
		static std::vector<outlierScore> calculateOutlierScores(
			std::vector<cluster> clusters,
			std::vector<double> pointNoiseLevels,
			std::vector<int> pointLastClusters,
			std::vector<double> coreDistances)
		{
			int numPoints = pointNoiseLevels.size();
			std::vector<outlierScore> outlierScores(numPoints);

			//Iterate through each point, calculating its outlier score:
			for (int i = 0; i < numPoints; i++)
			{
				double epsilonMax = clusters[pointLastClusters[i]].PropagatedLowestChildDeathLevel;
				double epsilon = pointNoiseLevels[i];
				double score = 0;

				if (epsilon != 0)
					score = 1 - (epsilonMax / epsilon);

				outlierScores.push_back(outlierScore(score, coreDistances[i], i));
			}
			//Sort the outlier scores:
			sort(outlierScores.begin(), outlierScores.end());

			return outlierScores;
		}

		/// <summary>
		/// Removes the set of points from their parent Cluster, and creates a new Cluster, provided the
		/// clusterId is not 0 (noise).
		/// </summary>
		/// <param name="points">The set of points to be in the new Cluster</param>
		/// <param name="clusterLabels">An array of cluster labels, which will be modified</param>
		/// <param name="parentCluster">The parent Cluster of the new Cluster being created</param>
		/// <param name="clusterLabel">The label of the new Cluster </param>
		/// <param name="edgeWeight">The edge weight at which to remove the points from their previous Cluster</param>
		/// <returns>The new Cluster, or null if the clusterId was 0</returns>
		static cluster createNewCluster(
			std::set<int> points,
			std::vector<int> clusterLabels,
			cluster parentCluster,
			int clusterLabel,
			double edgeWeight)
		{
			std::set<int>::iterator it = points.begin();
			while(it!= points.end())
			{
				clusterLabels[*it] = clusterLabel;
				++it;
			}

			parentCluster.detachPoints(points.size(), edgeWeight);

			if (clusterLabel != 0)
				return cluster(clusterLabel, parentCluster, edgeWeight, points.size());

			parentCluster.addPointsToVirtualChildCluster(points);
		}
		/// <summary>
		/// Calculates the number of constraints satisfied by the new clusters and virtual children of the
		/// parents of the new clusters.
		/// </summary>
		/// <param name="newClusterLabels">Labels of new clusters</param>
		/// <param name="clusters">An List of clusters</param>
		/// <param name="constraints">An List of constraints</param>
		/// <param name="clusterLabels">An array of current cluster labels for points</param>
		static void calculateNumConstraintsSatisfied(
			std::set<int> newClusterLabels,
			std::vector<cluster> clusters,
			std::vector<hdbscanConstraint> constraints,
			std::vector<int> clusterLabels)
		{
			if (constraints.size() == 0)
				return;

			std::vector<cluster> parents;
			std::vector<cluster> ::iterator it;
			for(int label : newClusterLabels)
			{
				cluster *parent = clusters[label].Parent;
				if (parent != NULL && !(find(parents.begin(),parents.end(),*parent)!=parents.end()))
					parents.push_back(*parent);
			}

			for(hdbscanConstraint constraint : constraints)
			{
				int labelA = clusterLabels[constraint.getPointA()];
				int labelB = clusterLabels[constraint.getPointB()];

				if (constraint.getConstraintType() == hdbscanConstraintType::mustLink && labelA == labelB)
				{
					if (find(newClusterLabels.begin(),newClusterLabels.end(),labelA)!=newClusterLabels.end())
						clusters[labelA].addConstraintsSatisfied(2);
				}
				else if (constraint.getConstraintType() == hdbscanConstraintType::cannotLink && (labelA != labelB || labelA == 0))
				{
					if (labelA != 0 && find(newClusterLabels.begin(), newClusterLabels.end(), labelA) != newClusterLabels.end())
						clusters[labelA].addConstraintsSatisfied(1);
					if (labelB != 0 && (find(newClusterLabels.begin(), newClusterLabels.end(), labelA) != newClusterLabels.end()))
						clusters[labelB].addConstraintsSatisfied(1);
					if (labelA == 0)
					{
						for(cluster parent : parents)
						{
							if (parent.virtualChildClusterConstraintsPoint(constraint.getPointA()))
							{
								parent.addVirtualChildConstraintsSatisfied(1);
								break;
							}
						}
					}
					if (labelB == 0)
					{
						for (cluster parent : parents)
						{
							if (parent.virtualChildClusterConstraintsPoint(constraint.getPointB()))
							{
								parent.addVirtualChildConstraintsSatisfied(1);
								break;
							}
						}
					}
				}
			}

			for(cluster parent : parents)
			{
				parent.releaseVirtualChildCluster();
			}
		}
	};

}

